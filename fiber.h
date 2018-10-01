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

#ifndef FIBER_H
#define FIBER_H

#include "libcoro/coro.h"
#include "queue.h"
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#ifdef FIBER_DEBUG
#  include <stdio.h>
#endif

struct coro {
	struct coro_context ctx;
	void *stack, *mmap;
	size_t stack_size, mmap_size;
	void *w;
};


struct fiber {
	struct coro coro;
	struct fiber *caller;
	long long id;

	SLIST_ENTRY(fiber) link, zombie_link;
	TAILQ_ENTRY(fiber) wake_link;
	void *wake;
	char cancel;

	char * top_of_stack;
	char * bottom_of_stack;
	uintptr_t last_retaddr;
	intptr_t * gc_regs;
	char * exception_pointer;
	struct caml__roots_block * caml_local_roots;
	int caml_backtrace_pos;
	void ** caml_backtrace_buffer;
	intptr_t caml_backtrace_last_exn;

	char name[20];
	uintptr_t cb, arg;
	uintptr_t ret;
};

extern struct fiber *fiber, *sched;
extern struct coro_context *sched_ctx;

#ifdef FIBER_TRACE
void fiber_resume(struct fiber *callee, void *w);
void *fiber_yield(void);
#define resume(callee, w) ({					\
	fprintf(stderr, "resume: %lli/%s -> %lli/%s arg:%p\n",	\
		  fiber->id, fiber->name, callee->id, callee->name, w); \
	fiber_resume(callee, w);				\
	})
#define yield() ({						\
	fprintf(stderr, "yield: %lli/%s -> %lli/%s\n",		\
		  fiber->id, fiber->name,			\
		  fiber->caller->id, fiber->caller->name);	\
	void *yield_ret = fiber_yield();			\
	fprintf(stderr, "yield: return arg:%p\n", yield_ret);	\
	yield_ret;						\
	})
#else
void resume(struct fiber *callee, void *w);
void *yield(void);
#endif

#endif
