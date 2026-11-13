/*
 * PS2 addresses and 32-bit pointer slots (docs/superpowers/specs/2026-11-11-build-64bit-pc-design.md).
 *
 * The game's structures keep the EE's layout: their pointer members are 4 bytes wide (ps2ptr_t)
 * whatever the host's pointer size. A slot holds a PS2 address:
 *
 *   P32(T, slot)   slot -> host pointer of type T (0 -> NULL)
 *   S32(ptr)       host pointer -> slot (NULL -> 0)
 *   PS2_ADDR(ptr)  host pointer -> PS2 address, for DMA tags and address arithmetic
 *   PS2_HOST(addr) PS2 address -> host pointer
 *
 * Three memory models:
 *   32-bit host            all four are plain casts.
 *   64-bit, fixed image    the executable is linked at the PS2 window (0x30000000): casts too, but a
 *                          host pointer that does not fit 32 bits stops the program (ps2_ptr_fail)
 *                          instead of being cut silently.
 *   64-bit, RECVX_PIE      the executable loads anywhere. A PS2 address is relative:
 *                            0x30000000 + n          byte n of the image (code, data, bss)
 *                            below 0x80000000        the stacks of the threads that hand out pointers
 *                                                    to their locals (16 MB each, registered on first use)
 *                            0x70000000..0x70003FFF  the scratchpad
 *                          Anything else below 4 GB passes unchanged both ways: an integer, or a value
 *                          that already is a PS2 address. A host pointer outside these areas (heap
 *                          memory) has no PS2 address: ps2_ptr_fail.
 * The game's "no pointer" marker -1 keeps its meaning both ways: (void*)-1 <-> 0xFFFFFFFF.
 */
#pragma once
#include <stdint.h>

typedef uint32_t ps2ptr_t;

#define PS2_WINDOW      0x30000000u
#define PS2_IMAGE_SPAN  0x10000000u   /* the image has the whole window, 256 MB */
#define PS2_STACK_TOP   0x80000000u   /* the stacks end here: bits 28 and 29 set, as in the window, so the */
#define PS2_STACK_SPAN  0x01000000u   /* EE's uncached aliases (addr | 0x20000000, | 0x30000000) change nothing */
#define PS2_STACK_MAX   8

#ifdef SLOTIFY_PARSE
/* pc/tools/slotify.py parses with these as functions: libclang gives no usable source range for
   an expression inside a macro argument */
static inline uint32_t PS2_ADDR(const void* p) { return (uint32_t)(uintptr_t)p; }
static inline void* PS2_HOST(uint32_t a) { return (void*)(uintptr_t)a; }
#else

#if UINTPTR_MAX == 0xFFFFFFFFu
#define PS2_ADDR(p) ((uint32_t)(uintptr_t)(p))
#define PS2_HOST(a) ((void*)(uintptr_t)(uint32_t)(a))
#else
#ifdef __cplusplus
extern "C" {
#endif
__attribute__((noreturn)) void ps2_ptr_fail(const void* p, const char* file, int line);
#ifdef RECVX_PIE
uint32_t ps2_addr_slow(uintptr_t v, const char* file, int line);
void* ps2_host_slow(uint32_t a);
#ifdef _WIN32
extern char __ImageBase;
#define PS2_IMAGE_BASE ((uintptr_t)&__ImageBase)
#else
extern char __executable_start;
#define PS2_IMAGE_BASE ((uintptr_t)&__executable_start)
#endif
#endif
#ifdef __cplusplus
}
#endif

/* takes the pointer as an integer so that PS2_ADDR also accepts what already is an address
   (a slot, an int holding one): both convert to uintptr_t without a diagnostic */
static inline uint32_t ps2_addr_(uintptr_t v, const char* file, int line)
{
#ifdef RECVX_PIE
    uintptr_t n = v - PS2_IMAGE_BASE;
    if (__builtin_expect(n < PS2_IMAGE_SPAN, 1)) return PS2_WINDOW + (uint32_t)n;
    if ((v >> 32) == 0) return (uint32_t)v;
    return ps2_addr_slow(v, file, line);
#else
    if (__builtin_expect((v >> 32) != 0, 0) && (v >> 32) != 0xFFFFFFFFu) ps2_ptr_fail((const void*)v, file, line);
    return (uint32_t)v;
#endif
}
static inline void* ps2_host_(uint32_t a)
{
#ifdef RECVX_PIE
    uint32_t n = a - PS2_WINDOW;
    if (__builtin_expect(n < PS2_IMAGE_SPAN, 1)) return (void*)(PS2_IMAGE_BASE + n);
    return ps2_host_slow(a);
#else
    return a == 0xFFFFFFFFu ? (void*)UINTPTR_MAX : (void*)(uintptr_t)a;
#endif
}
#define PS2_ADDR(p) ps2_addr_((uintptr_t)(p), __FILE__, __LINE__)
#define PS2_HOST(a) ps2_host_((uint32_t)(a))
#endif
#endif /* SLOTIFY_PARSE */

#define S32(p)    PS2_ADDR(p)
#define P32(T, s) ((T)PS2_HOST(s))

/* arithmetic on a slot, in units of the pointee as on the pointer it stands for (slotify.py):
   X->p += n, X->p++, ++X->p, X->p--, --X->p. The slot is evaluated more than once. */
#define P32_ADD(T, s, n)   ((s) = S32(P32(T, (s)) + (n)))
#define P32_PREINC(T, s)   P32(T, P32_ADD(T, s, 1))
#define P32_PREDEC(T, s)   P32(T, P32_ADD(T, s, -1))
#define P32_POSTINC(T, s)  (P32(T, P32_ADD(T, s, 1)) - 1)
#define P32_POSTDEC(T, s)  (P32(T, P32_ADD(T, s, -1)) + 1)
