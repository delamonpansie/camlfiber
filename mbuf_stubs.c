/*
 * Copyright (C) 2018 Yuriy Vostrikov
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


#include <caml/memory.h>
#include <caml/custom.h>
#include <caml/alloc.h>
#include <caml/threads.h>
#include <caml/fail.h>
#include <caml/unixsupport.h>

#include <assert.h>
#include <memory.h>
#include <sys/uio.h>
#include <errno.h>

#include "fiber.h"
#include "fiber_ev.h"

#define nelem(x) (sizeof((x))/sizeof((x)[0]))
#define unlikely(x)  __builtin_expect(!!(x),0)
#define likely(x)  __builtin_expect(!!(x),1)
#define max(a,b) ({ __typeof__ (a) _a = (a); \
                    __typeof__ (b) _b = (b); \
                    _a > _b ? _a : _b; })
#define min(a,b) ({ __typeof__ (a) _a = (a); \
                    __typeof__ (b) _b = (b); \
                    _a < _b ? _a : _b; })

struct blob {
        struct blob *prev, *next;
        char *start, *end;
        char _data[0];
};

/* hopefully malloc will be able to do nice and round allocation */
const int blob_capa = 64 * 1024 - sizeof(struct blob) - 8 * sizeof(void *);

struct blob_tailq {
        struct blob *head, *tail;
        int lenght;
};

static void
tailq_remove(struct blob_tailq *q, struct blob *b) {
        struct blob *prev = b->prev,
                    *next = b->next;
        if (!prev && !next) {
                q->head = q->tail = NULL;
        } else if (!prev) {
                q->head = next;
                next->prev = NULL;
        } else if (!next) {
                q->tail = prev;
                prev->next = NULL;
        } else {
                prev->next = next;
                next->prev = prev;
        }
        q->lenght--;
}

static void
tailq_concat(struct blob_tailq *dst, struct blob_tailq *src) {
        if (dst->tail) {
                dst->tail->next = src->head;
                src->head->prev = dst->tail;
                dst->tail = src->tail;
        } else {
                dst->head = src->head;
                dst->tail = src->tail;
        }

        src->head = src->tail = NULL;
        dst->lenght += src->lenght;
        src->lenght = 0;
}

static int
tailq_empty(const struct blob_tailq *a) {
        return a->lenght == 0;
}

static void
tailq_insert_tail(struct blob_tailq *q, struct blob *b) {
        if (q->tail) {
                q->tail->next = b;
                b->prev = q->tail;
                b->next = NULL;
                q->tail = b;
        } else {
                q->head = q->tail = b;
                b->prev = b->next = NULL;
        }
        q->lenght++;
}

#define tailq_foreach(var, q)                                           \
        for (struct blob *var = (q)->head; (var); (var) = (var)->next)
#define tailq_foreach_reverse_safe(var, q)                              \
        for (struct blob *var = (q)->tail, *__prev;                     \
             (var) && (__prev = (var)->prev, 1);                        \
             (var) = __prev)
#define tailq_foreach_safe(var, q)	                                \
        for (struct blob *var = (q)->head, *__next;                     \
             (var) && (__next = (var)->next, 1);                        \
             (var) = __next)

static int blob_cache_size;
static struct blob_tailq blob_cache;

static struct blob *blob_alloc() {
        struct blob *b = blob_cache.head;
        if (b != NULL) {
                tailq_remove(&blob_cache, b);
                blob_cache_size--;
        } else {
                b = malloc(blob_capa + sizeof(*b));
        }
        caml_alloc_dependent_memory(blob_capa);
        b->end = b->start = b->_data;
        return b;
}

static int blob_avail(const struct blob *b) {
        return blob_capa - (b->end - b->start);
}

static int blob_size(const struct blob *b) {
        return b->end - b->start;
}

value stub_mbuf_blob_size(value unit) {
        return Val_int(blob_capa);
}

struct mbuf {
        int size;
        struct blob_tailq q;
};

#define Mb(v) ((struct mbuf *)Data_custom_val(v))

void
mbuf_assert(const struct mbuf *c)
{
        if (c->q.head == NULL) {
                assert(c->size == 0);
                assert(c->q.tail == NULL);
                assert(c->q.lenght == 0);
        } else {
                assert(c->q.head->end >= c->q.head->start);
                int size = 0, qlen = 0;
                tailq_foreach(b, &c->q) {
                        size += blob_size(b);
                        qlen++;
                }
                assert(c->size == size);
                assert(c->q.lenght == qlen);
        }
}

static void
mbuf_finalize(value v) {
	struct mbuf *c = Data_custom_val(v);
        caml_free_dependent_memory(c->q.lenght * blob_capa);
        if (!tailq_empty(&c->q))
                tailq_concat(&blob_cache, &c->q);
}

static struct custom_operations mbuf_op = {
        .identifier = "Mbuf.t",
        .finalize = mbuf_finalize
};

static struct blob *
mbuf_extend(struct mbuf *c)
{
        struct blob *b = blob_alloc();
        tailq_insert_tail(&c->q, b);
        return b;
}

static void
mbuf_reduce(struct mbuf *c, struct blob *b)
{
        tailq_remove(&c->q, b);
        tailq_insert_tail(&blob_cache, b);
        caml_free_dependent_memory(blob_capa);
}

static void
mbuf_blit_from(const struct mbuf *c, int ofs, char *ptr, int n)
{
        struct blob *blob = c->q.head;
        char *data = blob->start;

        while (ofs) {
                int len = min(ofs, blob->end - data);
                ofs -= len;
                data += len;
                if (data == blob->end) {
                        blob = blob->next;
                        data = blob->start;
                }
        }

        while (n) {
                int len = min(n, blob->end - data);

                memcpy(ptr, data, len);
                ptr += len;

                n -= len;
                data += len;
                if (data == blob->end) {
                        blob = blob->next;
                        data = blob->start;
                }
        };
}

static void
mbuf_ltrim(struct mbuf *c, char *ptr, int n)
{
        struct blob *blob = c->q.head;
        char *data = blob->start;
        assert(c->size >= n);
        c->size -= n;
        do {
                int len = min(n, blob->end - data);
                if (ptr) {
                        memcpy(ptr, data, len);
                        ptr += len;
                }
                n -= len;
                data += len;
                if (data == blob->end) {
                        struct blob *next = blob->next;
                        mbuf_reduce(c, blob);
                        blob = next;
                        if (!blob) {
                                assert(n == 0);
                                break;
                        }
                        data = blob->start;
                }
        } while (n);
        if (blob) {
                blob->start = data;
        } else {
                assert(c->size  == 0);
                assert(c->q.lenght == 0);
        }
}

value
stub_mbuf_alloc(value unit)
{
        value v = caml_alloc_custom(&mbuf_op, sizeof(struct mbuf), 0, 1);
        struct mbuf *c = Data_custom_val(v);
        memset(c, 0, sizeof(*c));
        return v;
}

value
stub_mbuf_blit(value mb, value mbofs, value dst, value dstoff, value n)
{
        // TODO: argument validation
        mbuf_blit_from(Mb(mb), Int_val(mbofs),
                       String_val(dst) + Int_val(dstoff),
                       Int_val(n));
        return Val_unit;
}

value
stub_mbuf_clear(value mb)
{
        struct mbuf *c = Mb(mb);
        tailq_foreach_reverse_safe(b, &c->q)
                mbuf_reduce(c, b);
        memset(c, 0, sizeof(*c));
        return Val_unit;
}

value
stub_mbuf_transfer(value dst_val, value src_val)
{
        struct mbuf *dst = Mb(dst_val),
                    *src = Mb(src_val);

        if (tailq_empty(&src->q))
                return Val_unit;

        tailq_concat(&dst->q, &src->q);
        dst->size += src->size;
        src->size = 0;

        mbuf_assert(dst);
        mbuf_assert(src);
        return Val_unit;
}

static int
mbuf_size(const struct mbuf *c)
{
        return c->size;
}

value
stub_mbuf_size(value mb)
{
        struct mbuf *c = Mb(mb);
        return Val_int(mbuf_size(c));
}

value
stub_mbuf_capacity(value mb)
{
        struct mbuf *c = Mb(mb);
        return Val_int(c->q.lenght * blob_capa);
}

value
stub_mbuf_extend(value mb)
{
        struct mbuf *c = Mb(mb);
        mbuf_extend(c);
        return Val_unit;
}

static void  __attribute__((noinline))
mbuf_take(struct mbuf *c, void *ptr, int n)
{
        if (n > 0)
                mbuf_ltrim(c, ptr, n);
}

static void *
mbuf_take_small(struct mbuf *c, int n)
{
        char *ptr = c->q.head->start;
        c->q.head->start += n;
        c->size -= n;
        return ptr;
}

value
stub_mbuf_take_char(value mb)
{
        struct mbuf *c = Mb(mb);
        /* stricly less because if equal, we must
           free blob after reading and we won't
           do this in a fast path */
        unsigned char v;
        if (likely(c->q.head->start + 1 < c->q.head->end))
                v = *(unsigned char *)mbuf_take_small(c, sizeof(v));
        else
                mbuf_take(c, &v, sizeof(v));
        return Val_int(v);
}

value
stub_mbuf_take_int8(value mb)
{
        struct mbuf *c = Mb(mb);
        char v;
        if (likely(c->q.head->start + 1 < c->q.head->end))
                v = *(char *)mbuf_take_small(c, sizeof(v));
        else
                mbuf_take(c, &v, sizeof(v));
        return Val_int(v);
}

value
stub_mbuf_take_int(value mb)
{
        struct mbuf *c = Mb(mb);
        int32_t v;
        if (likely(c->q.head->start + 4 < c->q.head->end))
                v = *(int32_t *)mbuf_take_small(c, sizeof(v));
        else
                mbuf_take(c, &v, sizeof(v));
        return Val_int(v);
}

value
stub_mbuf_take_int32(value mb)
{
        struct mbuf *c = Mb(mb);
        int32_t v;
        if (likely(c->q.head->start + 4 < c->q.head->end))
                v = *(int32_t *)mbuf_take_small(c, sizeof(v));
        else
                mbuf_take(c, &v, sizeof(v));
        return caml_copy_int32(v);
}

value
stub_mbuf_take_int64(value mb)
{
        struct mbuf *c = Mb(mb);
        int64_t v;
        if (likely(c->q.head->start + 8 < c->q.head->end))
                v = *(int64_t *)mbuf_take_small(c, sizeof(v));
        else
                mbuf_take(c, &v, sizeof(v));
        return caml_copy_int64(v);
}

value
stub_mbuf_take_bytes(value mb, value len)
{
        CAMLparam2(mb, len);
        CAMLlocal1(s);
        s = caml_alloc_string(Int_val(len));
        struct mbuf *c = Mb(mb);
        if (mbuf_size(c) < Int_val(len))
                caml_invalid_argument("Mbuf.take_bytes");
        mbuf_take(c, Bytes_val(s), Int_val(len));
        CAMLreturn(s);
}


static void  __attribute__((noinline))
mbuf_put(struct mbuf *c, void *ptr, int n)
{
        struct blob *blob = c->q.tail;
        do {
                int len = blob_avail(blob);
                if (len > n)
                        len = n;
                memcpy(blob->end, ptr, len);
                ptr += len;
                n -= len;
                blob->end += len;
                if (blob_avail(blob) == 0)
                        blob = mbuf_extend(c);
        } while (n);
}

value
stub_mbuf_take_mbuf(value mb, value lenv)
{
        CAMLparam2(mb, lenv);
        CAMLlocal1(ret);
        ret = stub_mbuf_alloc(Val_unit);
        int len = Int_val(lenv);
        struct mbuf *src = Mb(mb),
                    *dst = Mb(ret);

        if (mbuf_size(src) < len)
                caml_invalid_argument("Mbuf.take_mbuf");

        src->size -= len;
        dst->size += len;

        if (len < blob_size(src->q.head)) {
                mbuf_extend(dst);
                mbuf_put(dst, src->q.head->start, len);
                src->q.head->start += len;
        } else {
                tailq_foreach_safe(b, &src->q) {
                        if (len == 0 || len < blob_size(b))
                                break;
                        tailq_remove(&src->q, b);
                        tailq_insert_tail(&dst->q, b);
                        len -= blob_size(b);
                }
                if (len > 0) {
                        if (blob_avail(dst->q.tail) < len)
                                mbuf_extend(dst);
                        mbuf_put(dst, src->q.head->start, len);
                        src->q.head->start += len;
                }
        }
        /* mbuf_assert(src); */
        /* mbuf_assert(dst); */
        CAMLreturn(ret);
}

#define PUT(c, i)  ({           			\
       if (likely(blob_avail((c)->q.tail) < sizeof(i))) {       \
               typeof(i) *end = (void *)(c)->q.tail->end;               \
               (c)->q.tail->end += sizeof(i);                           \
               (c)->size += sizeof(i);                                  \
               *end = i;                                                \
       } else {                                                         \
               typeof(i) __i_storage = i;                               \
               mbuf_put((c), &__i_storage, sizeof(i));                  \
               (c)->size += sizeof(i);                                  \
       }                                                                \
})

value
stub_mbuf_put_int8(value mb, value v)
{
        struct mbuf *c = Mb(mb);
        PUT(c, (char)Int_val(v));
        return Val_unit;
}

value
stub_mbuf_put_int(value mb, value v)
{
        struct mbuf *c = Mb(mb);
        PUT(c, Int_val(v));
        return Val_unit;
}

value
stub_mbuf_put_int32(value mb, value v)
{
        struct mbuf *c = Mb(mb);
        PUT(c, Int32_val(v));
        return Val_unit;
}

value
stub_mbuf_put_int64(value mb, value v)
{
        struct mbuf *c = Mb(mb);
        PUT(c, Int64_val(v));
        return Val_unit;
}

value
stub_mbuf_put_bytes(value mb, value v)
{
        struct mbuf *c = Mb(mb);
        int len = caml_string_length(v);
        mbuf_put(c, String_val(v), len);
        c->size += len;
        return Val_unit;
}

static void  __attribute__((noinline))
mbuf_blit(struct mbuf *c, int ofs, void *ptr, int n)
{
        struct blob *blob = c->q.head;
        void *dst = c->q.head->start,
             *end = c->q.head->end;
        while (ofs) {
                if (ofs > end - dst) {
                        ofs -= end - dst;
                        blob = blob->next;
                        dst = blob->start;
                        end = blob->end;
                } else {
                        dst += ofs;
                        break;
                }
        }

        while (n) {
                int len = min(end - dst, n);
                memcpy(dst, ptr, len);
                ptr += len;
                n -= len;
                blob = blob->next;
                if (!blob) {
                        assert(n == 0);
                        break;
                }
                dst = blob->start;
                end = blob->end;
        }
}


value
stub_mbuf_blit_char(value mb, value ofsv, value v)
{
        struct mbuf *c = Mb(mb);
        int ofs = Int_val(ofsv);
        char x = Int_val(v);
        if (ofs + sizeof(x) > c->size)
                caml_invalid_argument("Mbuf.blit_char");
        mbuf_blit(c, ofs, &x, sizeof(x));
        return Val_unit;
}

value
stub_mbuf_blit_int(value mb, value ofsv, value v)
{
        struct mbuf *c = Mb(mb);
        int ofs = Int_val(ofsv);
        int32_t x = Int_val(v);
        if (ofs + sizeof(x) > c->size)
                caml_invalid_argument("Mbuf.blit_int");
        mbuf_blit(c, ofs, &x, sizeof(x));
        return Val_unit;
}

value
stub_mbuf_blit_int32(value mb, value ofsv, value v)
{
        struct mbuf *c = Mb(mb);
        int ofs = Int_val(ofsv);
        int32_t x = Int32_val(v);
        if (ofs + sizeof(x) > c->size)
                caml_invalid_argument("Mbuf.blit_int32");
        mbuf_blit(c, ofs, &x, sizeof(x));
        return Val_unit;
}

value
stub_mbuf_blit_int64(value mb, value ofsv, value v)
{
        struct mbuf *c = Mb(mb);
        int ofs = Int_val(ofsv);
        int64_t x = Int64_val(v);
        if (ofs + sizeof(x) > c->size)
                caml_invalid_argument("Mbuf.blit_int64");
        mbuf_blit(c, ofs, &x, sizeof(x));
        return Val_unit;
}

value
stub_mbuf_blit_float(value mb, value ofsv, value v)
{
        struct mbuf *c = Mb(mb);
        int ofs = Int_val(ofsv);
        double x = Double_val(v);
        if (ofs + sizeof(x) > c->size)
                caml_invalid_argument("Mbuf.blit_float");
        mbuf_blit(c, ofs, &x, sizeof(x));
        return Val_unit;
}

value
stub_mbuf_blit_bytes(value mb, value ofsv, value v, value bofsv, value lenv)
{
        struct mbuf *c = Mb(mb);
        int len = Int_val(lenv);
        int ofs = Int_val(ofsv);
        char *x = String_val(v) + Int_val(bofsv);
        if (Int_val(bofsv) + len > caml_string_length(v))
                caml_invalid_argument("Mbuf.blit_bytes");
        if (ofs + len > c->size)
                caml_invalid_argument("Mbuf.blit_bytes");
        mbuf_blit(c, ofs, &x, len);
        return Val_unit;
}


static int
iovec_for_recv(struct blob *b, struct iovec *iov) {
        struct iovec *start = iov;
        for (; b; b = b->next, iov++) {
                iov->iov_base = b->start;
                iov->iov_len = blob_avail(b);
        }
        return iov - start;
}

static int
iovec_for_send(struct mbuf *c, struct iovec *iov) {
        struct iovec *start = iov;
        tailq_foreach(b, &c->q) {
                iov->iov_base = b->start;
                iov->iov_len = blob_size(b);
                iov++;
        }
        return iov - start;
}

static void
adjust_iovec(struct iovec **iov, int *iovcnt, int size)
{
        while (*iovcnt) {
                if ((*iov)->iov_len < size) {
                        size -= (*iov)->iov_len;
                        (*iov)++;
                        (*iovcnt)--;
                } else {
                        (*iov)->iov_base += size;
                        (*iov)->iov_len -= size;
                        break;
                }
        }
}

int
mbuf_recv(int fd, struct mbuf *c, int readahead) {
        /* extend with at least one blob */
        do {
                mbuf_extend(c);
                readahead -= blob_capa;
        } while (readahead > 0);

        struct blob *tail = c->q.tail;
        struct iovec iov_buf[c->q.lenght], *iov = iov_buf;
        int iovcnt = iovec_for_recv(tail, iov);
	ev_io io = { .coro = 1 };
	ev_io_init(&io, (void *)fiber, fd, EV_READ);
	ev_io_start(&io);
        yield();
        int size = readv(fd, iov, iovcnt);
	ev_io_stop(&io);

        int total = size;
        if (size > 0) {
                c->size += size;
                for (struct blob *b = tail; b; b = b->next) {
                        int avail = blob_avail(b);
                        if (total > avail) {
                                b->end += avail;
                                total -= avail;
                        } else {
                                b->end += total;
                                break;
                        }
                }
        }
        tailq_foreach_reverse_safe(b, &c->q) {
                if (blob_size(b) == 0)
                        mbuf_reduce(c, b);
        }

        return total;
}

int
mbuf_send(int fd, struct mbuf *c) {
        struct iovec iov_buf[c->q.lenght], *iov = iov_buf;
        int iovcnt = iovec_for_send(c, iov);
        int sent = 0;

        mbuf_assert(c);
        int sz = 0;
        for (int i = 0; i < iovcnt; i++)
                sz += iov[i].iov_len;
        assert(sz == c->size);

        ev_io io = { .coro = 1 };
	ev_io_init(&io, (void *)fiber, fd, EV_WRITE);
	ev_io_start(&io);

        int size = mbuf_size(c);
        while (size != sent) {
                int s = writev(fd, iov, iovcnt);
                if (s == size) {
                        sent = s;
                        break;
                }
                if (s <= 0 &&
                    errno != EAGAIN &&
                    errno != EWOULDBLOCK &&
                    errno != EINTR) {
                        if (sent == 0)
                                sent = s; // propagate error
                        break;
                }
                sent += s;
                adjust_iovec(&iov, &iovcnt, s);
                yield();
        }
        ev_io_stop(&io);
        if (sent > 0)
                mbuf_ltrim(c, NULL, sent);
        return sent;
}

value
stub_mbuf_recv(value fd, value mb, value readahead) {
        CAMLparam2(mb, fd);
        if (fiber->id == 1)
                caml_invalid_argument("Mbuf.recv");
        struct mbuf c;
        memcpy(&c, Data_custom_val(mb), sizeof(c));
	caml_enter_blocking_section();
	int n = mbuf_recv(Int_val(fd), &c, Int_val(readahead));
	caml_leave_blocking_section();
        memcpy(Data_custom_val(mb), &c, sizeof(c));
        if (n == 0)
                caml_raise_end_of_file();
        if (n < 0)
                uerror("readv", Nothing);
	CAMLreturn(Val_unit);
}

value
stub_mbuf_send(value fd, value mb) {
        CAMLparam2(mb, fd);
        if (fiber->id == 1)
                caml_invalid_argument("Mbuf.send");
        struct mbuf c;
        memcpy(&c, Data_custom_val(mb), sizeof(c));
	caml_enter_blocking_section();
	int n = mbuf_send(Int_val(fd), &c);
	caml_leave_blocking_section();
        memcpy(Data_custom_val(mb), &c, sizeof(c));
        if (n < 0)
                uerror("writev", Nothing);
	CAMLreturn(Val_int(n));
}
