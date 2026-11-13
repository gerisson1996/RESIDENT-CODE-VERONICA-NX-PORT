#ifndef _WIN32
#define _GNU_SOURCE
#endif
/*
 * PS2 address-space setup.
 *
 * Memory model (docs/ARCHITECTURE.md "Data model and memory"): the executable is linked at 0x30000000 so that
 * the EE's uncached / uncached-accelerated aliases (addr|0x20000000,
 * addr|0x30000000) are identities on every game object, and DMA tags can hold
 * plain 31-bit host addresses. The 16 KB scratchpad is mapped at its real EE
 * address, 0x70000000, because the platform code builds packets at literal
 * scratchpad addresses.
 */
#ifdef _WIN32
/* the PS2 kernel shim declares its own GetThreadId/CreateThread */
#define GetThreadId win_GetThreadId
#define CreateThread win_CreateThread
#include <windows.h>
#undef GetThreadId
#undef CreateThread
#endif
#include "ps2_platform.h"
#include "pc_ptr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__SWITCH__)
/* libnx, without <switch.h> (its names clash with the PS2 shim's) */
typedef struct { uint64_t addr, size; uint32_t type, attr, perm, ipc_refcount, device_refcount, padding; } nx_meminfo;
extern uint32_t svcQueryMemory(nx_meminfo* info, uint32_t* pageinfo, uint64_t addr);
#elif !defined(_WIN32)
#include <sys/mman.h>
#include <link.h>
#include <unistd.h>
#endif

Ps2FakeRegs ps2_regs;

#if !defined(_WIN32) && !defined(__SWITCH__)
/* The decomp writes into string literals (no MMU protection on the EE). Make the
   executable's read-only data writable. */
static int unprotect_cb(struct dl_phdr_info* info, size_t sz, void* data)
{
    (void)sz; (void)data;
    if (info->dlpi_name && info->dlpi_name[0]) return 0; /* main executable only */
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)* ph = &info->dlpi_phdr[i];
        if (ph->p_type == PT_LOAD && !(ph->p_flags & PF_W) && !(ph->p_flags & PF_X)) {
            uintptr_t page = (uintptr_t)sysconf(_SC_PAGESIZE);
            uintptr_t a = (info->dlpi_addr + ph->p_vaddr) & ~(page - 1);
            uintptr_t e = info->dlpi_addr + ph->p_vaddr + ph->p_memsz;
            if (mprotect((void*)a, e - a, PROT_READ | PROT_WRITE) != 0) perror("mprotect(rodata)");
        }
    }
    return 1;
}
#endif

/* The scratchpad's address is taken: name what holds it (on Windows usually a DLL another program
   injects, an overlay or an antivirus), so the player knows what to close */
static void spr_taken(void)
{
    char who[512] = "";
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((void*)(uintptr_t)PS2_SPR_BASE, &mbi, sizeof(mbi)) && mbi.Type == MEM_IMAGE)
        GetModuleFileNameA((HMODULE)mbi.AllocationBase, who, sizeof(who));
#else
    FILE* f = fopen("/proc/self/maps", "r");
    char line[600];
    unsigned long a, e;
    while (f && fgets(line, sizeof(line), f))
        if (sscanf(line, "%lx-%lx", &a, &e) == 2 && a < PS2_SPR_BASE + PS2_SPR_SIZE && e > PS2_SPR_BASE) {
            char* p = strchr(line, '/');
            if (p) { p[strcspn(p, "\n")] = 0; snprintf(who, sizeof(who), "%s", p); }
            break;
        }
    if (f) fclose(f);
#endif
    if (who[0]) host_fatal("Cannot reserve game memory at 0x%08X: it is used by\n%s\n\nClose the program that adds it (an overlay or antivirus), or restart the computer.", PS2_SPR_BASE, who);
    host_fatal("Cannot reserve game memory at 0x%08X. Restart the computer and try again.", PS2_SPR_BASE);
}

void ps2_memory_init(void)
{
#ifdef _WIN32
#ifndef RECVX_PIE
    void* spr = VirtualAlloc((void*)(uintptr_t)PS2_SPR_BASE, PS2_SPR_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (spr != (void*)(uintptr_t)PS2_SPR_BASE) spr_taken();
#endif
    {   /* the decomp writes into string literals: make the image's read-only data sections writable */
        uint8_t* base = (uint8_t*)GetModuleHandleA(NULL);
        IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
        IMAGE_SECTION_HEADER* sh = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; i++, sh++) {
            DWORD old, ch = sh->Characteristics;
            if (!(ch & IMAGE_SCN_MEM_READ) || (ch & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))) continue;
            if (!VirtualProtect(base + sh->VirtualAddress, sh->Misc.VirtualSize, PAGE_READWRITE, &old))
                host_fatal("cannot make section %.8s writable", (const char*)sh->Name);
        }
    }
#elif defined(__SWITCH__)
    /* the string literals the decomp writes to are linked into a writable segment (pc/switch/recvx.ld) */
#else
#ifndef RECVX_PIE
    void* spr = mmap((void*)(uintptr_t)PS2_SPR_BASE, PS2_SPR_SIZE, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (spr != (void*)(uintptr_t)PS2_SPR_BASE) spr_taken();
#endif
    dl_iterate_phdr(unprotect_cb, NULL);
#endif
    {
        extern char ps2_window_probe;
        uintptr_t end = 0;
#ifdef _WIN32
        uint8_t* base = (uint8_t*)GetModuleHandleA(NULL);
        IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
        IMAGE_SECTION_HEADER* sh = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; i++, sh++)   /* not .reloc, .debug_* */
            if (!(sh->Characteristics & IMAGE_SCN_MEM_DISCARDABLE) && (uintptr_t)base + sh->VirtualAddress + sh->Misc.VirtualSize > end)
                end = (uintptr_t)base + sh->VirtualAddress + sh->Misc.VirtualSize;
#elif defined(__SWITCH__)
        extern char __end__;
        end = (uintptr_t)&__end__;
#else
        extern char _end;
        end = (uintptr_t)&_end;
#endif
#ifdef RECVX_PIE
        /* the image loads anywhere; PS2 address 0x30000000 is its first byte (pc_ptr.h) */
        if (end - PS2_IMAGE_BASE > PS2_IMAGE_SPAN)
            host_fatal("the executable takes %u MB, more than the %u MB of PS2 addresses kept for it",
                       (unsigned)((end - PS2_IMAGE_BASE) >> 20), (unsigned)(PS2_IMAGE_SPAN >> 20));
        host_log("memory: image at %p = PS2 0x%08X, %u MB%c", (void*)PS2_IMAGE_BASE, PS2_WINDOW, (unsigned)((end - PS2_IMAGE_BASE) >> 20), 10);
        end = 0;
#endif
        if (!end) ; else
        if (((uintptr_t)&ps2_window_probe & 0x30000000u) != 0x30000000u)
            host_fatal("executable is not linked inside the PS2 address window (0x30000000)");
        /* all static game memory (room arena, texture heap, ...) must lie in the window: the budgets
           in pc_prelude.h share the 256 MB up to 0x40000000 (docs/modding/ROOM_FORMAT.md "Limits and budgets") */
        if (end > 0x40000000u)
            host_fatal("the executable's data end at 0x%08X, past the PS2 address window (0x40000000)", (unsigned)end);
    }
}
char ps2_window_probe;

/* DMA MADR/TADR/ADDR field -> host pointer */
#ifdef RECVX_PIE
/* a host pointer cut to 32 bits instead of going through PS2_ADDR: its low bits are those of the
   image's; logged once per caller (link-relative return address: addr2line -e recvx.exe) */
static void pie_trunc_check(uint32_t addr, void* ra, const char* what)
{
    static uintptr_t seen[64]; static int nseen;
    if ((uint32_t)(addr - (uint32_t)PS2_IMAGE_BASE) >= PS2_IMAGE_SPAN || addr - PS2_WINDOW < PS2_IMAGE_SPAN) return;
    uintptr_t r = (uintptr_t)ra - PS2_IMAGE_BASE + 0x140000000u;
    for (int i = 0; i < nseen; i++) if (seen[i] == r) return;
    if (nseen < 64) seen[nseen++] = r;
    host_log("pie: truncated host pointer %08X in %s from 0x%llx%c", addr, what, (unsigned long long)r, 10);
}
#endif

void* ps2_dma_ptr(uint32_t addr)
{
#ifdef RECVX_PIE
    pie_trunc_check(addr, __builtin_return_address(0), "dma");
#endif
    if (addr & 0x80000000u) return PS2_HOST(PS2_SPR_BASE | (addr & (PS2_SPR_SIZE - 1)));
#ifdef RECVX_PIE
    if (addr >= PS2_STACK_TOP - PS2_STACK_MAX * PS2_STACK_SPAN) return PS2_HOST(addr);   /* a stack (pc_ptr.h) */
#endif
    if ((addr & 0xF0000000u) == 0x70000000u) return PS2_HOST(PS2_SPR_BASE | (addr & (PS2_SPR_SIZE - 1)));
    if (addr < 0x10000000u) addr |= 0x30000000u; /* physical address (masked with 0x0FFFFFFF) */
    return PS2_HOST(addr);
}

#if UINTPTR_MAX != 0xFFFFFFFFu
#ifdef __SWITCH__
#include <unwind.h>
extern char __start__[];
struct trace { uintptr_t* pcs; int* n; };
static _Unwind_Reason_Code trace_one(struct _Unwind_Context* c, void* ud)
{
    struct trace* t = ud;
    if (*t->n >= 12) return _URC_END_OF_STACK;
    t->pcs[(*t->n)++] = (uintptr_t)_Unwind_GetIP(c);
    return _URC_NO_REASON;
}
#endif
/* a host pointer that does not fit a 32-bit slot or DMA field (pc_ptr.h) */
void ps2_ptr_fail(const void* p, const char* file, int line)
{
#ifdef __SWITCH__
    {   /* the callers, for addr2line -e recvx.elf (linked at 0) */
        uintptr_t pcs[12];
        int n = 0;
        _Unwind_Backtrace(trace_one, &(struct trace){ pcs, &n });
        for (int i = 0; i < n; i++) host_log("memory: bad pointer, caller %d at 0x%llx%c", i, (unsigned long long)(pcs[i] - (uintptr_t)__start__), 10);
    }
#endif
    host_fatal("host pointer %p does not fit a PS2 address (%s:%d)", p, file, line);
    abort();
}
#endif

#ifdef RECVX_PIE
/* ---- relative PS2 addresses (pc_ptr.h): what is not in the image ---- */
static unsigned char ps2_spr[PS2_SPR_SIZE] __attribute__((aligned(64)));   /* the scratchpad */
static struct { uintptr_t lo, hi; } ps2_stacks[PS2_STACK_MAX];             /* host stacks: slot i ends at PS2_STACK_TOP - i * PS2_STACK_SPAN */
static volatile int ps2_stack_n;
#ifdef _WIN32
static CRITICAL_SECTION ps2_stack_lock;
static volatile LONG ps2_stack_lock_ready;
#else
#include <pthread.h>
static pthread_mutex_t ps2_stack_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

/* the calling thread's stack: [lo, hi) */
static int this_stack(uintptr_t* lo, uintptr_t* hi)
{
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION mbi;
    char probe;
    if (!VirtualQuery(&probe, &mbi, sizeof(mbi))) return 0;
    *lo = (uintptr_t)mbi.AllocationBase;
    *hi = (uintptr_t)((NT_TIB*)NtCurrentTeb())->StackBase;
    return 1;
#elif defined(__SWITCH__)
    /* libnx threads (pthreads, SDL's) have their stack allocated from the heap: the thread's
       own record gives it; the main thread's stack is one mapping of its own */
    struct nx_thread { uint32_t handle; unsigned char owns; void *mem, *mirror; size_t size; };   /* libnx's Thread */
    extern struct nx_thread* threadGetSelf(void);
    nx_meminfo mi;
    uint32_t page;
    char probe;
    struct nx_thread* t = threadGetSelf();
    if (t && t->size) {
        uintptr_t p = (uintptr_t)&probe;
        void* bases[2] = { t->mirror, t->mem };
        for (int k = 0; k < 2; k++)
            if (bases[k] && p >= (uintptr_t)bases[k] && p < (uintptr_t)bases[k] + t->size) {
                *lo = (uintptr_t)bases[k]; *hi = *lo + t->size;
                return 1;
            }
    }
    if (svcQueryMemory(&mi, &page, (uint64_t)(uintptr_t)&probe) != 0) return 0;
    if (mi.size > PS2_STACK_SPAN) {
        /* a stack inside a larger mapping (the heap, or the loader's): a window of the span
           around this frame, half above for the callers' frames, half below for deeper calls */
        uintptr_t p = (uintptr_t)&probe & ~(uintptr_t)0xFFF, end = (uintptr_t)(mi.addr + mi.size);
        *hi = p + PS2_STACK_SPAN / 2 < end ? p + PS2_STACK_SPAN / 2 : end;
        *lo = *hi - PS2_STACK_SPAN > (uintptr_t)mi.addr ? *hi - PS2_STACK_SPAN : (uintptr_t)mi.addr;
        host_log("memory: thread stack in a %llu KB mapping (thread record %p, %zu bytes); using %p..%p%c",
                 (unsigned long long)(mi.size >> 10), t ? t->mem : NULL, t ? t->size : (size_t)0, (void*)*lo, (void*)*hi, 10);
        return 1;
    }
    *lo = (uintptr_t)mi.addr; *hi = (uintptr_t)(mi.addr + mi.size);
    return 1;
#else
    pthread_attr_t at; void* base; size_t size;
    if (pthread_getattr_np(pthread_self(), &at) != 0) return 0;
    pthread_attr_getstack(&at, &base, &size);
    pthread_attr_destroy(&at);
    *lo = (uintptr_t)base; *hi = *lo + size;
    return 1;
#endif
}

uint32_t ps2_addr_slow(uintptr_t v, const char* file, int line)
{
    uintptr_t lo, hi;
    int i, n;
    if ((v >> 32) == 0xFFFFFFFFu) return (uint32_t)v;   /* -1 and other negative ints */
    if (v - (uintptr_t)ps2_spr < PS2_SPR_SIZE) return PS2_SPR_BASE + (uint32_t)(v - (uintptr_t)ps2_spr);
    for (i = 0, n = ps2_stack_n; i < n; i++)
        if (v >= ps2_stacks[i].lo && v < ps2_stacks[i].hi) goto found;
    if (!this_stack(&lo, &hi) || v < lo || v >= hi) ps2_ptr_fail((const void*)v, file, line);
    if (hi - lo > PS2_STACK_SPAN) lo = hi - PS2_STACK_SPAN;   /* the deepest part of a larger stack has no address */
    if (v < lo) ps2_ptr_fail((const void*)v, file, line);
#ifdef _WIN32
    if (InterlockedCompareExchange(&ps2_stack_lock_ready, 1, 0) == 0) { InitializeCriticalSection(&ps2_stack_lock); ps2_stack_lock_ready = 2; }
    while (ps2_stack_lock_ready != 2) Sleep(0);
    EnterCriticalSection(&ps2_stack_lock);
#else
    pthread_mutex_lock(&ps2_stack_lock);
#endif
    for (i = 0; i < ps2_stack_n && ps2_stacks[i].hi != hi; i++);
    if (i == ps2_stack_n) {
        if (i == PS2_STACK_MAX) host_fatal("more than %d threads hand out pointers to their stacks (%s:%d)", PS2_STACK_MAX, file, line);
        ps2_stacks[i].lo = lo; ps2_stacks[i].hi = hi;
        ps2_stack_n = i + 1;
        host_log("memory: stack %d at %p = PS2 0x%08X%c", i, (void*)hi, PS2_STACK_TOP - i * PS2_STACK_SPAN, 10);
    }
#ifdef _WIN32
    LeaveCriticalSection(&ps2_stack_lock);
#else
    pthread_mutex_unlock(&ps2_stack_lock);
#endif
found:
    return PS2_STACK_TOP - (uint32_t)i * PS2_STACK_SPAN - (uint32_t)(ps2_stacks[i].hi - v);
}

void* ps2_host_slow(uint32_t a)
{
    if (a == 0) return NULL;
    pie_trunc_check(a, __builtin_return_address(0), "PS2_HOST");
    if (a == 0xFFFFFFFFu) return (void*)UINTPTR_MAX;
    if (a - PS2_SPR_BASE < PS2_SPR_SIZE) return ps2_spr + (a - PS2_SPR_BASE);
    if (a >= PS2_STACK_TOP - PS2_STACK_MAX * PS2_STACK_SPAN && a < PS2_STACK_TOP) {
        unsigned i = (PS2_STACK_TOP - 1 - a) / PS2_STACK_SPAN;
        if ((int)i < ps2_stack_n) return (void*)(ps2_stacks[i].hi - ((PS2_STACK_TOP - i * PS2_STACK_SPAN) - a));
    }
    return (void*)(uintptr_t)a;   /* an integer, or an address this is not the place to judge */
}
#endif
