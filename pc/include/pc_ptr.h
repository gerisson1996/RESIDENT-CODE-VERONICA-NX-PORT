/*
 * PS2 addresses and 32-bit pointer slots (docs/superpowers/specs/2026-11-11-build-64bit-pc-design.md).
 *
 * Structures read from the disc keep the EE's layout: their pointer members are 4 bytes wide
 * (ps2ptr_t) whatever the host's pointer size. A slot holds a PS2 address:
 *
 *   P32(T, slot)   slot -> host pointer of type T (0 -> NULL)
 *   S32(ptr)       host pointer -> slot (NULL -> 0)
 *   PS2_ADDR(ptr)  host pointer -> PS2 address, for DMA tags and address arithmetic
 *   PS2_HOST(addr) PS2 address -> host pointer
 *
 * 32-bit build: all four are plain casts. 64-bit build with the image linked in the PS2 window
 * (0x30000000): casts too, but a host pointer that does not fit 32 bits stops the program
 * (ps2_ptr_fail) instead of being cut silently. The game's "no pointer" marker -1 keeps its
 * meaning both ways: (void*)-1 <-> 0xFFFFFFFF.
 */
#pragma once
#include <stdint.h>

typedef uint32_t ps2ptr_t;

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
extern "C"
#endif
__attribute__((noreturn)) void ps2_ptr_fail(const void* p, const char* file, int line);
/* takes the pointer as an integer so that PS2_ADDR also accepts what already is an address
   (a slot, an int holding one): both convert to uintptr_t without a diagnostic */
static inline uint32_t ps2_addr_(uintptr_t v, const char* file, int line)
{
    if (__builtin_expect((v >> 32) != 0, 0) && v != UINTPTR_MAX && (v >> 32) != 0xFFFFFFFFu) ps2_ptr_fail((const void*)v, file, line);
    return (uint32_t)v;
}
static inline void* ps2_host_(uint32_t a) { return a == 0xFFFFFFFFu ? (void*)UINTPTR_MAX : (void*)(uintptr_t)a; }
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
