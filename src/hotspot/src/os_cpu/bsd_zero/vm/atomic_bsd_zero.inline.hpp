/*
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2007, 2008, 2011 Red Hat, Inc.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef OS_CPU_BSD_ZERO_VM_ATOMIC_BSD_ZERO_INLINE_HPP
#define OS_CPU_BSD_ZERO_VM_ATOMIC_BSD_ZERO_INLINE_HPP

#include "orderAccess_bsd_zero.inline.hpp"
#include "runtime/atomic.hpp"
#include "runtime/os.hpp"
#include "vm_version_zero.hpp"

#include <sys/types.h>
#ifdef __NetBSD__
#include <sys/atomic.h>
#elif __FreeBSD__

#include <sys/types.h>
#ifndef SPARC
#include <machine/atomic.h>
#else

/*
 * On FreeBSD/sparc64, <machine/atomic.h> pulls in <machine/cpufunc.h>
 * which includes definitions which cause conflicts with various
 * definitions within HotSpot source.  To avoid that, pull in those
 * definitions verbatim instead of including the header.  Yuck.
 */

/*-
 * Copyright (c) 1998 Doug Rabson.
 * Copyright (c) 2001 Jake Burkholder.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <machine/asi.h>

/*
 * Membar operand macros for use in other macros when # is a special
 * character.  Keep these in sync with what the hardware expects.
 */
#define	M_LoadLoad	(0)
#define	M_StoreLoad	(1)
#define	M_LoadStore	(2)
#define	M_StoreStore	(3)

#define	CMASK_SHIFT	(4)
#define	MMASK_SHIFT	(0)

#define	CMASK_GEN(bit)	((1 << (bit)) << CMASK_SHIFT)
#define	MMASK_GEN(bit)	((1 << (bit)) << MMASK_SHIFT)

#define	LoadLoad	MMASK_GEN(M_LoadLoad)
#define	StoreLoad	MMASK_GEN(M_StoreLoad)
#define	LoadStore	MMASK_GEN(M_LoadStore)
#define	StoreStore	MMASK_GEN(M_StoreStore)

#define	casa(rs1, rs2, rd, asi) ({					\
	u_int __rd = (uint32_t)(rd);					\
	__asm __volatile("casa [%2] %3, %4, %0"				\
	    : "+r" (__rd), "=m" (*rs1)					\
	    : "r" (rs1), "n" (asi), "r" (rs2), "m" (*rs1));		\
	__rd;								\
})

#define	casxa(rs1, rs2, rd, asi) ({					\
	u_long __rd = (uint64_t)(rd);					\
	__asm __volatile("casxa [%2] %3, %4, %0"			\
	    : "+r" (__rd), "=m" (*rs1)					\
	    : "r" (rs1), "n" (asi), "r" (rs2), "m" (*rs1));		\
	__rd;								\
})

#define	membar(mask) do {						\
	__asm __volatile("membar %0" : : "n" (mask) : "memory");	\
} while (0)

#ifdef _KERNEL
#define	__ASI_ATOMIC	ASI_N
#else
#define	__ASI_ATOMIC	ASI_P
#endif

#define mb()	__asm__ __volatile__ ("membar #MemIssue": : :"memory")
#define wmb()	mb()
#define rmb()	mb()

/*
 * Various simple arithmetic on memory which is atomic in the presence
 * of interrupts and multiple processors.  See atomic(9) for details.
 * Note that efficient hardware support exists only for the 32 and 64
 * bit variants; the 8 and 16 bit versions are not provided and should
 * not be used in MI code.
 *
 * This implementation takes advantage of the fact that the sparc64
 * cas instruction is both a load and a store.  The loop is often coded
 * as follows:
 *
 *	do {
 *		expect = *p;
 *		new = expect + 1;
 *	} while (cas(p, expect, new) != expect);
 *
 * which performs an unnnecessary load on each iteration that the cas
 * operation fails.  Modified as follows:
 *
 *	expect = *p;
 *	for (;;) {
 *		new = expect + 1;
 *		result = cas(p, expect, new);
 *		if (result == expect)
 *			break;
 *		expect = result;
 *	}
 *
 * the return value of cas is used to avoid the extra reload.
 *
 * The memory barriers provided by the acq and rel variants are intended
 * to be sufficient for use of relaxed memory ordering.  Due to the
 * suggested assembly syntax of the membar operands containing a #
 * character, they cannot be used in macros.  The cmask and mmask bits
 * are hard coded in machine/cpufunc.h and used here through macros.
 * Hopefully sun will choose not to change the bit numbers.
 */

#define	itype(sz)	uint ## sz ## _t

#define	atomic_cas_32(p, e, s)	casa(p, e, s, __ASI_ATOMIC)
#define	atomic_cas_64(p, e, s)	casxa(p, e, s, __ASI_ATOMIC)

#define	atomic_cas(p, e, s, sz)						\
	atomic_cas_ ## sz(p, e, s)

#define	atomic_cas_acq(p, e, s, sz) ({					\
	itype(sz) v;							\
	v = atomic_cas(p, e, s, sz);					\
	membar(LoadLoad | LoadStore);					\
	v;								\
})

#define	atomic_cas_rel(p, e, s, sz) ({					\
	itype(sz) v;							\
	membar(LoadStore | StoreStore);					\
	v = atomic_cas(p, e, s, sz);					\
	v;								\
})

#define	atomic_op(p, op, v, sz) ({					\
	itype(sz) e, r, s;						\
	for (e = *(volatile itype(sz) *)p;; e = r) {			\
		s = e op v;						\
		r = atomic_cas_ ## sz(p, e, s);				\
		if (r == e)						\
			break;						\
	}								\
	e;								\
})

#define	atomic_op_acq(p, op, v, sz) ({					\
	itype(sz) t;							\
	t = atomic_op(p, op, v, sz);					\
	membar(LoadLoad | LoadStore);					\
	t;								\
})

#define	atomic_op_rel(p, op, v, sz) ({					\
	itype(sz) t;							\
	membar(LoadStore | StoreStore);					\
	t = atomic_op(p, op, v, sz);					\
	t;								\
})

#define	atomic_load(p, sz)						\
	atomic_cas(p, 0, 0, sz)

#define	atomic_load_acq(p, sz) ({					\
	itype(sz) v;							\
	v = atomic_load(p, sz);						\
	membar(LoadLoad | LoadStore);					\
	v;								\
})

#define	atomic_load_clear(p, sz) ({					\
	itype(sz) e, r;							\
	for (e = *(volatile itype(sz) *)p;; e = r) {			\
		r = atomic_cas(p, e, 0, sz);				\
		if (r == e)						\
			break;						\
	}								\
	e;								\
})

#define	atomic_store(p, v, sz) do {					\
	itype(sz) e, r;							\
	for (e = *(volatile itype(sz) *)p;; e = r) {			\
		r = atomic_cas(p, e, v, sz);				\
		if (r == e)						\
			break;						\
	}								\
} while (0)

#define	atomic_store_rel(p, v, sz) do {					\
	membar(LoadStore | StoreStore);					\
	atomic_store(p, v, sz);						\
} while (0)

#define	ATOMIC_GEN(name, ptype, vtype, atype, sz)			\
									\
static __inline vtype							\
atomic_add_ ## name(volatile ptype p, atype v)				\
{									\
	return ((vtype)atomic_op(p, +, v, sz));				\
}									\
static __inline vtype							\
atomic_add_acq_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op_acq(p, +, v, sz));			\
}									\
static __inline vtype							\
atomic_add_rel_ ## name(volatile ptype p, atype v)			\
{									\
	return ((vtype)atomic_op_rel(p, +, v, sz));			\
}									\
									\
static __inline int							\
atomic_cmpset_ ## name(volatile ptype p, vtype e, vtype s)		\
{									\
	return (((vtype)atomic_cas(p, e, s, sz)) == e);			\
}									\
static __inline int							\
atomic_cmpset_acq_ ## name(volatile ptype p, vtype e, vtype s)		\
{									\
	return (((vtype)atomic_cas_acq(p, e, s, sz)) == e);		\
}									\
static __inline int							\
atomic_cmpset_rel_ ## name(volatile ptype p, vtype e, vtype s)		\
{									\
	return (((vtype)atomic_cas_rel(p, e, s, sz)) == e);		\
}									\
									\
static __inline vtype							\
atomic_load_ ## name(volatile ptype p)					\
{									\
	return ((vtype)atomic_cas(p, 0, 0, sz));			\
}									\
static __inline vtype							\
atomic_load_acq_ ## name(volatile ptype p)				\
{									\
	return ((vtype)atomic_cas_acq(p, 0, 0, sz));			\
}									\
									\
static __inline void							\
atomic_store_ ## name(volatile ptype p, vtype v)			\
{									\
	atomic_store(p, v, sz);						\
}									\
static __inline void							\
atomic_store_rel_ ## name(volatile ptype p, vtype v)			\
{									\
	atomic_store_rel(p, v, sz);					\
}

inline jlong Atomic::load(volatile jlong* src) {
  volatile jlong dest;
  os::atomic_copy64(src, &dest);
  return dest;
}

inline void Atomic::store(jlong store_value, jlong* dest) {
  os::atomic_copy64((volatile jlong*)&store_value, (volatile jlong*)dest);
}

inline void Atomic::store(jlong store_value, volatile jlong* dest) {
  os::atomic_copy64((volatile jlong*)&store_value, dest);
}

ATOMIC_GEN(int, u_int *, u_int, u_int, 32);
ATOMIC_GEN(32, uint32_t *, uint32_t, uint32_t, 32);

ATOMIC_GEN(long, u_long *, u_long, u_long, 64);
ATOMIC_GEN(64, uint64_t *, uint64_t, uint64_t, 64);

ATOMIC_GEN(ptr, uintptr_t *, uintptr_t, uintptr_t, 64);

#define	atomic_fetchadd_int	atomic_add_int
#define	atomic_fetchadd_32	atomic_add_32
#define	atomic_fetchadd_long	atomic_add_long

#undef ATOMIC_GEN
#undef atomic_cas
#undef atomic_cas_acq
#undef atomic_cas_rel
#undef atomic_op
#undef atomic_op_acq
#undef atomic_op_rel
#undef atomic_load_acq
#undef atomic_store_rel
#undef atomic_load_clear
#endif

static __inline __attribute__((__always_inline__))
unsigned int atomic_add_int_nv(volatile unsigned int* dest, unsigned int add_value)
{
  atomic_add_acq_int(dest, add_value);
  return *dest;
}

static __inline __attribute__((__always_inline__))
uintptr_t atomic_add_ptr_nv(volatile intptr_t* dest, intptr_t add_value)
{
  atomic_add_acq_ptr((volatile uintptr_t*) dest, (uintptr_t) add_value);
  return *((volatile uintptr_t*) dest);
}

static __inline __attribute__((__always_inline__))
unsigned int
atomic_swap_uint(volatile unsigned int *dest, unsigned int exchange_value)
{
  jint prev = *dest;
  atomic_store_rel_int(dest, exchange_value);
  return prev;
}

static __inline __attribute__((__always_inline__))
void *
atomic_swap_ptr(volatile void *dest, void *exchange_value)
{
  void *prev = *(void **)dest;
  atomic_store_rel_ptr((volatile uintptr_t*) dest, (uintptr_t) exchange_value);
  return prev;
}

static __inline __attribute__((__always_inline__))
unsigned int
atomic_cas_uint(volatile unsigned int *dest, unsigned int compare_value,
  unsigned int exchange_value)
{
  unsigned int prev = *dest;
  atomic_cmpset_acq_int(dest, compare_value, exchange_value);
  return prev;
}

static __inline __attribute__((__always_inline__))
unsigned long
atomic_cas_ulong(volatile unsigned long *dest, unsigned long compare_value,
  unsigned long exchange_value)
{
  unsigned long prev = *dest;
  atomic_cmpset_acq_long(dest, compare_value, exchange_value);
  return prev;
}

static __inline __attribute__((__always_inline__))
void *
atomic_cas_ptr(volatile void *dest, void *compare_value, void *exchange_value)
{
  void *prev = *(void **)dest;
  atomic_cmpset_acq_ptr((volatile uintptr_t*) dest, (uintptr_t) compare_value, (uintptr_t) exchange_value);
  return prev;
}

#elif defined(__APPLE__)

#include <libkern/OSAtomic.h>

static __inline __attribute__((__always_inline__))
unsigned int
atomic_add_int_nv(volatile unsigned int *target, int delta) {
  return (unsigned int) OSAtomicAdd32Barrier(delta, (volatile int32_t *) target);
}

static __inline __attribute__((__always_inline__))
void *
atomic_add_ptr_nv(volatile void *target, ssize_t delta) {
#ifdef __LP64__
  return (void *) OSAtomicAdd64Barrier(delta, (volatile int64_t *) target);
#else
  return (void *) OSAtomicAdd32Barrier(delta, (volatile int32_t *) target);
#endif
}


static __inline __attribute__((__always_inline__))
unsigned int
atomic_swap_uint(volatile unsigned int *dest, unsigned int exchange_value)
{
  /* No xchg support in OSAtomic */
  unsigned int prev;
  do {
    prev = *dest;
  } while (!OSAtomicCompareAndSwapIntBarrier((int) prev, (int) exchange_value, (volatile int *) dest));

  return prev;
}

static __inline __attribute__((__always_inline__))
void *
atomic_swap_ptr(volatile void *dest, void *exchange_value)
{
  /* No xchg support in OSAtomic */
  void *prev;
  do {
    prev = *((void * volatile *) dest);
  } while (!OSAtomicCompareAndSwapPtrBarrier(prev, exchange_value, (void * volatile *) dest));

  return prev;
}

static __inline __attribute__((__always_inline__))
unsigned int
atomic_cas_uint(volatile unsigned int *dest, unsigned int compare_value,
  unsigned int exchange_value)
{
  unsigned int prev = *dest;
  OSAtomicCompareAndSwapIntBarrier(compare_value, exchange_value, (volatile int *) dest);
  return prev;
}

static __inline __attribute__((__always_inline__))
unsigned long
atomic_cas_ulong(volatile unsigned long *dest, unsigned long compare_value,
  unsigned long exchange_value)
{
  unsigned long prev = *dest;
  OSAtomicCompareAndSwapLongBarrier(compare_value, exchange_value, (volatile long *) dest);
  return prev;
}

static __inline __attribute__((__always_inline__))
void *
atomic_cas_ptr(volatile void *dest, void *compare_value, void *exchange_value)
{
  void *prev = *(void **)dest;
  OSAtomicCompareAndSwapPtrBarrier(compare_value, exchange_value, (void * volatile *) dest);
  return prev;
}


#endif

inline void Atomic::store(jint store_value, volatile jint* dest) {
  *dest = store_value;
}

inline void Atomic::store_ptr(intptr_t store_value, intptr_t* dest) {
  *dest = store_value;
}

inline jint Atomic::add(jint add_value, volatile jint* dest) {
  return (jint)atomic_add_int_nv((volatile unsigned int*) dest, add_value);
}

inline intptr_t Atomic::add_ptr(intptr_t add_value, volatile intptr_t* dest) {
  return (intptr_t)atomic_add_ptr_nv(dest, add_value);
}

inline void* Atomic::add_ptr(intptr_t add_value, volatile void* dest) {
  return (void *) add_ptr(add_value, (volatile intptr_t *) dest);
}

inline void Atomic::inc(volatile jint* dest) {
  add(1, dest);
}

inline void Atomic::inc_ptr(volatile intptr_t* dest) {
  add_ptr(1, dest);
}

inline void Atomic::inc_ptr(volatile void* dest) {
  add_ptr(1, dest);
}

inline void Atomic::dec(volatile jint* dest) {
  add(-1, dest);
}

inline void Atomic::dec_ptr(volatile intptr_t* dest) {
  add_ptr(-1, dest);
}

inline void Atomic::dec_ptr(volatile void* dest) {
  add_ptr(-1, dest);
}

inline jint Atomic::xchg(jint exchange_value, volatile jint* dest) {
  return (jint)atomic_swap_uint((volatile u_int *)dest, (u_int)exchange_value);
}

inline intptr_t Atomic::xchg_ptr(intptr_t exchange_value,
                                 volatile intptr_t* dest) {
  return (intptr_t)atomic_swap_ptr((volatile void *)dest,
    (void *)exchange_value);
}

inline void* Atomic::xchg_ptr(void* exchange_value, volatile void* dest) {
  return atomic_swap_ptr(dest, exchange_value);
}

inline jint Atomic::cmpxchg(jint exchange_value,
                            volatile jint* dest,
                            jint compare_value) {
  return atomic_cas_uint((volatile u_int *)dest, compare_value, exchange_value);
}

inline jlong Atomic::cmpxchg(jlong exchange_value,
                             volatile jlong* dest,
                             jlong compare_value) {
  return atomic_cas_ulong((volatile u_long *)dest, compare_value,
    exchange_value);
}

inline intptr_t Atomic::cmpxchg_ptr(intptr_t exchange_value,
                                    volatile intptr_t* dest,
                                    intptr_t compare_value) {
  return (intptr_t)atomic_cas_ptr((volatile void *)dest, (void *)compare_value,
      (void *)exchange_value);
}

inline void* Atomic::cmpxchg_ptr(void* exchange_value,
                                 volatile void* dest,
                                 void* compare_value) {
  return atomic_cas_ptr((volatile void *)dest, compare_value, exchange_value);
}

#endif // OS_CPU_BSD_ZERO_VM_ATOMIC_BSD_ZERO_INLINE_HPP
