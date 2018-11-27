/*
 * Copyright (C) 2010-2016 Mail.RU
 * Copyright (C) 2010-2018 Yuriy Vostrikov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
// #include <sys/time.h>
#include <unistd.h>

#include "queue.h"

#include <caml/memory.h>
#include <caml/callback.h>
#include <caml/custom.h>
#include <caml/alloc.h>
#include <caml/threads.h>
#include <caml/fail.h>

#include "fiber.h"
#include "fiber_ev.h"
#include "libcoro/coro.h"

#if HAVE_VALGRIND_VALGRIND_H && !defined(NVALGRIND)
# include <valgrind/valgrind.h>
# include <valgrind/memcheck.h>
#else
# define VALGRIND_MAKE_MEM_DEFINED(_qzz_addr, _qzz_len) (void)0
# define VALGRIND_MAKE_MEM_UNDEFINED(_qzz_addr, _qzz_len) (void)0
# define VALGRIND_MALLOCLIKE_BLOCK(addr, sizeB, rzB, is_zeroed) (void)0
# define VALGRIND_FREELIKE_BLOCK(addr, rzB) (void)0
# define VALGRIND_MAKE_MEM_NOACCESS(_qzz_addr,_qzz_len) (void)0
# define VALGRIND_STACK_REGISTER(_qzz_addr,_qzz_len) (void)0
#endif

#define MH_SOURCE 1
#define mh_name _long
#define mh_key_t long
#define mh_val_t void *
#include "mhash.h"

static struct coro *
coro_alloc(struct coro *coro, void (*f) (void *), void *data)
{
	const int page = sysconf(_SC_PAGESIZE);

	assert(coro != NULL);
	memset(coro, 0, sizeof(*coro));

	coro->mmap_size = page * 128;
	coro->mmap = mmap(NULL, coro->mmap_size, PROT_READ | PROT_WRITE | PROT_EXEC,
			  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	if (coro->mmap == MAP_FAILED)
		goto fail;

        /* 16 pages of no access guard zone */
	if (mprotect(coro->mmap, page * 16, PROT_NONE) < 0)
		goto fail;

	(void)VALGRIND_MAKE_MEM_NOACCESS(coro->mmap, page * 16);

	const int red_zone_size = sizeof(void *) * 4;
	coro->stack = coro->mmap + 16 * page;
	coro->stack_size = coro->mmap_size - 16 * page - red_zone_size;
	void **red_zone = coro->stack + coro->stack_size;
	red_zone[0] = red_zone[1] = NULL;
	red_zone[2] = red_zone[3] = (void *)(uintptr_t)0xDEADDEADDEADDEADULL;

	(void)VALGRIND_STACK_REGISTER(coro->stack, coro->stack + coro->stack_size);
	coro_create(&coro->ctx, f, data, coro->stack, coro->stack_size);
	return coro;

	int saved_errno;
fail:
	saved_errno = errno;
	coro_destroy(coro);
	errno = saved_errno;
	return NULL;
}


SLIST_HEAD(, fiber) fibers, zombie_fibers;


struct fiber* sched;
coro_context *sched_ctx = NULL;
struct fiber *fiber = NULL;
static unsigned long last_used_id = 100; /* ids below 99 are reserved */


static ev_prepare wake_prep;
static ev_async wake_async;

static struct mh_long_t *fibers_registry;

static TAILQ_HEAD(, fiber) wake_list;

#if defined(FIBER_TRACE) || defined(FIBER_EV_DEBUG)
void
fiber_ev_cb(void *arg)
{
	fprintf(stderr, "%s: =<< arg:%p\n", __func__, arg);
}
#endif

static void
fiber_async_send()
{
        ev_async_send(&wake_async);
}

static void
fiber_async(ev_async* ev __attribute__((unused)),
            int events __attribute__((unused)))
{
}

void
#ifdef FIBER_TRACE
fiber_resume(struct fiber *callee, void *w)
#else
resume(struct fiber *callee, void *w)
#endif
{
#ifndef FIBER_NDEBUG
	assert(callee != sched && callee->caller == NULL);
#endif
	struct fiber *caller = fiber;
	callee->caller = caller;
	fiber = callee;
	callee->coro.w = w;
	coro_transfer(&caller->coro.ctx, &callee->coro.ctx);
}

void *
#ifdef FIBER_TRACE
fiber_yield(void)
#else
yield(void)
#endif
{
	struct fiber *callee = fiber;
#ifndef FIBER_NDEBUG
        assert(callee->id != 1);
	assert(callee->caller != NULL);
#endif
	fiber = callee->caller;
	callee->caller = NULL;
	coro_transfer(&callee->coro.ctx, &fiber->coro.ctx);
	return fiber->coro.w;
}

int
fiber_wake(struct fiber *f, void *arg)
{
	assert(f != sched);
	/* tqe_prev points to prev elem or tailq head => not null if member */
	if (f->wake_link.tqe_prev)
		return 0;
#ifdef FIBER_TRACE
	fprintf(stderr, "%s: %lli/%s arg:%p\n", __func__, f->fid, f->name, arg);
#endif
	f->wake = arg;
	TAILQ_INSERT_TAIL(&wake_list, f, wake_link);
	fiber_async_send();
	return 1;
}

int
fiber_cancel_wake(struct fiber *f)
{
	assert(f != sched);
	/* see fiber_wake() comment */
	if (f->wake_link.tqe_prev == NULL)
		return 0;
	TAILQ_REMOVE(&wake_list, f, wake_link);
	f->wake_link.tqe_prev = NULL;
	return 1;
}

void
fiber_sleep(ev_tstamp delay)
{
	assert(fiber != sched);
	ev_timer *s, w = { .coro = 1 };
	ev_timer_init(&w, (void *)fiber, delay, 0.);
	ev_timer_start(&w);
	s = yield();
	assert(s == &w);
	(void)s;
	ev_timer_stop(&w);
}


struct fiber *
fid2fiber(int fid)
{
	unsigned k = mh_long_get(fibers_registry, fid);
	if (k == mh_end(fibers_registry))
		return NULL;
	return mh_long_value(fibers_registry, k);
}

static void
register_id(struct fiber *fiber)
{
	mh_long_put(fibers_registry, fiber->id, fiber, NULL);
}

static void
unregister_id(struct fiber *fiber)
{
	mh_long_remove(fibers_registry, fiber->id, NULL);
}


static void
fiber_zombificate(struct fiber *f)
{
	strcpy(f->name, "zombie");
	unregister_id(f);
	f->id = 0;
	// TODO: trash fiber->last_retaddr and friends
        // TODO: madvise()
	SLIST_INSERT_HEAD(&zombie_fibers, f, zombie_link);
}

static value
fiber_trampoline(value cb, value arg)
{
	CAMLparam2(cb, arg);
	CAMLlocal1(ret);
	caml_leave_blocking_section();
	ret = caml_callback(cb, arg);
	caml_enter_blocking_section();
	CAMLreturn(ret);
}

static void
fiber_loop(void *data __attribute__((unused)))
{
	while (42) {
		assert(fiber != NULL && fiber->id != 0);
		value cb = fiber->cb,
		     arg = fiber->arg;
		fiber->cb = fiber->arg = 0;
		fiber_trampoline(cb, arg);
		fiber_zombificate(fiber);
		yield();	/* give control back to scheduler */
	}
}

int
fiber_create(value cb, value arg)
{
	struct fiber *new = NULL;

	if (!SLIST_EMPTY(&zombie_fibers)) {
		new = SLIST_FIRST(&zombie_fibers);
		SLIST_REMOVE_HEAD(&zombie_fibers, zombie_link);
	} else {
		new = calloc(1, sizeof(struct fiber));
		if (coro_alloc(&new->coro, fiber_loop, NULL) == NULL) {
			perror("fiber_create");
			exit(1);
		}

		SLIST_INSERT_HEAD(&fibers, new, link);
	}

	new->id = last_used_id++; /* we believe that 2**63 won't overflow */
	register_id(new);

	new->cb = cb;
	new->arg = arg;
	memset(new->name, 0, sizeof(new->name));
	return new->id;
}

static void
fiber_wakeup_pending(void)
{
	assert(fiber == sched);
	struct fiber *f;

	for(int i=10; i && !TAILQ_EMPTY(&wake_list); i--) {
		TAILQ_INSERT_TAIL(&wake_list, sched, wake_link);
		while (1) {
			f = TAILQ_FIRST(&wake_list);
			TAILQ_REMOVE(&wake_list, f, wake_link);
			if (f == sched) break;
			f->wake_link.tqe_prev = NULL;
#ifdef FIBER_TRACE
			fprintf(stderr, "%s: %lli/%s arg:%p\n", __func__,
				f->fid, f->name, f->wake);
#endif
			resume(f, f->wake);
		}
	}

	if (!TAILQ_EMPTY(&wake_list))
		fiber_async_send();
}


typedef void (*scanning_action) (value, value *);

extern char * caml_top_of_stack;
extern char * caml_bottom_of_stack;
extern uintnat caml_last_return_address;
extern value * caml_gc_regs;
extern char * caml_exception_pointer;
extern int caml_backtrace_pos;
extern void * caml_backtrace_buffer;
extern value caml_backtrace_last_exn;

extern void (*caml_scan_roots_hook) (scanning_action);
extern void (*caml_enter_blocking_section_hook)(void);
extern void (*caml_leave_blocking_section_hook)(void);
extern int (*caml_try_leave_blocking_section_hook)(void);
extern uintnat (*caml_stack_usage_hook)(void);

extern void caml_do_local_roots(scanning_action f, char * bottom_of_stack,
				uintnat last_retaddr, value * gc_regs,
				struct caml__roots_block * local_roots);

static void (*prev_scan_roots_hook)(scanning_action);
static void (*prev_enter_blocking_section_hook)(void);
static void (*prev_leave_blocking_section_hook)(void);
static int (*prev_try_leave_blocking_section_hook)(void);
static uintnat (*prev_stack_usage_hook)(void);

static void fiber_scan_roots(struct fiber *f, scanning_action action)
{
	if (f->cb) {
		/* fiber is not fully initialized yet */
		assert(f->arg);
		(*action)(f->cb, &f->cb);
		(*action)(f->arg, &f->arg);
	} else {
		(*action)(f->backtrace_last_exn, &f->backtrace_last_exn);

		/* Don't rescan the stack of the current fiber, it was done already */
		if (f == fiber)
			return;

		assert(f->last_retaddr > 0xffff);
		caml_do_local_roots(action,
				    f->bottom_of_stack, f->last_retaddr,
				    f->gc_regs, f->local_roots);
	}
}

static void fiber_all_scan_roots(scanning_action action)
{
	struct fiber *f;
        fiber_scan_roots(sched, action);
	SLIST_FOREACH(f, &fibers, link) {
                if (f->id == 0) /* skip zombie */
                        continue;
                fiber_scan_roots(f, action);
        }

	if (prev_scan_roots_hook != NULL)
		(*prev_scan_roots_hook)(action);
}

static void
fiber_enter_blocking_section(void)
{
	fiber->top_of_stack = caml_top_of_stack;
	fiber->bottom_of_stack = caml_bottom_of_stack;
	fiber->last_retaddr = caml_last_return_address;
	fiber->gc_regs = caml_gc_regs;
	fiber->exception_pointer = caml_exception_pointer;
	fiber->local_roots = caml_local_roots;

	fiber->backtrace_pos = caml_backtrace_pos;
	fiber->backtrace_buffer = caml_backtrace_buffer;
	fiber->backtrace_last_exn = caml_backtrace_last_exn;

        caml_local_roots = (void *)0xdead;
	caml_last_return_address = 0xbeef;
}

static void
fiber_leave_blocking_section(void)
{
	caml_top_of_stack = fiber->top_of_stack;
	caml_bottom_of_stack= fiber->bottom_of_stack;
	caml_last_return_address = fiber->last_retaddr;
	caml_gc_regs = fiber->gc_regs;
	caml_exception_pointer = fiber->exception_pointer;
	caml_local_roots = fiber->local_roots;

	caml_backtrace_pos = fiber->backtrace_pos;
	caml_backtrace_buffer = fiber->backtrace_buffer;
	caml_backtrace_last_exn = fiber->backtrace_last_exn;

        fiber->local_roots = (void *)0xdead;
	fiber->last_retaddr = 0xbeef;
}

static int
fiber_try_leave_blocking_section(void)
{
	return 1;
}

static uintnat
fiber_stack_usage(void)
{
  uintnat sz = 0;
  struct fiber *f;

  SLIST_FOREACH(f, &fibers, link) {
          if (f->id == 0) /* skip zombie */
                  continue;
	  /* Don't add stack for current thread, this is done elsewhere */
	  if (f == fiber)
		  continue;
	  sz += (value *) f->top_of_stack - (value *) f->bottom_of_stack;
  }
  if (prev_stack_usage_hook != NULL)
	  sz += prev_stack_usage_hook();
  return sz;
}

static struct fiber *
Fiber_val(value fib)
{
	int fid = Int_val(Field(fib, 0));
	return fid2fiber(fid);
}

value
stub_fiber_sleep(value tm)
{
	CAMLparam1(tm);
	caml_enter_blocking_section();
	fiber_sleep(Double_val(tm));
	caml_leave_blocking_section();
	CAMLreturn(Val_unit);
}

value
stub_fiber_create(value cb, value arg)
{
	CAMLparam2(cb, arg);
	CAMLlocal1(fib);
	caml_enter_blocking_section();
	fib = Val_int(fiber_create(cb, arg));
	caml_leave_blocking_section();
	CAMLreturn(fib);
}

value
stub_fiber_run(value unit)
{
	caml_enter_blocking_section();
	ev_run(0);
	caml_leave_blocking_section();
	return Val_unit;
}

value
stub_yield(value init)
{
        if (fiber->id == 1)
                caml_invalid_argument("Fiber.yield");
	caml_enter_blocking_section();
	yield();
	caml_leave_blocking_section();
	return Val_unit;
}

value
stub_unsafe_yield(value unit)
{
        CAMLparam1(unit);
        CAMLlocal1(result);
        if (fiber->id == 1)
		caml_invalid_argument("Fiber.unsafe_yield");
        caml_enter_blocking_section();
	result = (intptr_t)yield();
	caml_leave_blocking_section();
        CAMLreturn(result);
}

value
stub_resume(value fib)
{
	struct fiber *f = Fiber_val(fib);
	if (f == NULL || f->caller != NULL)
		caml_invalid_argument("Fiber.resume");
	caml_enter_blocking_section();
	resume(f, NULL);
	caml_leave_blocking_section();
	return Val_unit;
}

value
stub_unsafe_resume(value fib, value arg)
{
        CAMLparam2(fib, arg);
	struct fiber *f = Fiber_val(fib);
	if (f == NULL || f->caller != NULL)
		caml_invalid_argument("Fiber.unsafe_resume");
	caml_enter_blocking_section();
	resume(f, (void *)arg);
	caml_leave_blocking_section();
	CAMLreturn(Val_unit);
}

value
stub_wake(value fid)
{
	struct fiber *f = fid2fiber(Int_val(fid));
	if (f == NULL)
		caml_invalid_argument("Fiber.wake");
	fiber_wake(f, NULL);
	return Val_unit;
}

value
stub_cancel_wake(value fid)
{
	struct fiber *f = fid2fiber(Int_val(fid));
	if (f == NULL)
		caml_invalid_argument("Fiber.cancel_wake");
	fiber_cancel_wake(f);
	return Val_unit;
}

value
stub_fiber_id(value unit)
{
	return Val_int(fiber->id);
}

value
stub_break(value unit)
{
	ev_break(EVBREAK_ALL);
	return Val_unit;
}

value
stub_wait_io_ready(value fd_value, value mode_value)
{
	int mode;
	switch (Int_val(mode_value)) {
	case 0: mode = EV_READ; break;
	case 1: mode = EV_WRITE; break;
	default: assert(0);
	}
 	ev_io io = { .coro = 1 };
	ev_io_init(&io, (void *)fiber, Int_val(fd_value), mode);
	ev_io_start(&io);
	stub_yield(Val_unit);
	ev_io_stop(&io);
	return Val_unit;
}


__attribute__((constructor))
static void
fiber_init(void)
{
	SLIST_INIT(&fibers);
	SLIST_INIT(&zombie_fibers);
	TAILQ_INIT(&wake_list);

	fibers_registry = mh_long_init(realloc);

	sched = calloc(1, sizeof(struct fiber));
	sched->id = 1;
	strcpy(sched->name, "sched");
	sched->last_retaddr = 0xbeef;
	sched_ctx = &sched->coro.ctx;

	fiber = sched;

        ev_default_loop(ev_recommended_backends() | EVFLAG_SIGNALFD);

	ev_prepare_init(&wake_prep, (void *)fiber_wakeup_pending);
	ev_set_priority(&wake_prep, -1);
	ev_prepare_start(&wake_prep);
	ev_async_init(&wake_async, fiber_async);
        ev_async_start(&wake_async);

	prev_scan_roots_hook = caml_scan_roots_hook;
        prev_enter_blocking_section_hook = caml_enter_blocking_section_hook;
        prev_leave_blocking_section_hook = caml_leave_blocking_section_hook;
        prev_try_leave_blocking_section_hook = caml_try_leave_blocking_section_hook;
        prev_stack_usage_hook = caml_stack_usage_hook;

        caml_scan_roots_hook = fiber_all_scan_roots;
	caml_enter_blocking_section_hook = fiber_enter_blocking_section;
	caml_leave_blocking_section_hook = fiber_leave_blocking_section;
	caml_try_leave_blocking_section_hook = fiber_try_leave_blocking_section;
	caml_stack_usage_hook = fiber_stack_usage;
}
