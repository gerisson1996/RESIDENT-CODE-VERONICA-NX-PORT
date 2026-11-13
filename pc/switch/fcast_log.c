/*
 * Diagnostic (Switch): the handler GCC's -fsanitize=float-cast-overflow calls when a float is
 * converted to an integer type it does not fit (pc/CMakeLists.txt). arm64 saturates there (a
 * negative value to an unsigned type gives 0) where x86, the port's reference, wraps; the result
 * stays arm64's. Each place is logged once with its value, so a log points at the conversions
 * that may change the game's behaviour.
 */
#include <stdint.h>
#include <string.h>

void host_log(const char* fmt, ...);

struct src_loc { const char* file; uint32_t line, col; };
struct type_desc { uint16_t kind, info; char name[1]; };
struct fcast_data { struct src_loc loc; const struct type_desc *from, *to; };

static double value(const struct type_desc* t, uintptr_t v)
{
    unsigned bits = t->info;
    if (bits == 32) { float f; uint32_t u = (uint32_t)v; memcpy(&f, &u, 4); return f; }
    if (bits == 64) { double d; uint64_t u = (uint64_t)v; memcpy(&d, &u, 8); return d; }
    return 0;
}

void __ubsan_handle_float_cast_overflow(struct fcast_data* d, uintptr_t from)
{
    static const char* seen_f[128];
    static uint32_t seen_l[128];
    static int n, total;
    const char* f = d->loc.file ? d->loc.file : "?";
    const char* s = strrchr(f, '/');
    int i;
    total++;
    for (i = 0; i < n; i++) if (seen_l[i] == d->loc.line && seen_f[i] == d->loc.file) return;
    if (n == 128) return;
    seen_f[n] = d->loc.file; seen_l[n] = d->loc.line; n++;
    host_log("fcast: %s:%u: %g to %s does not fit (arm64 saturates, x86 wraps)\n", s ? s + 1 : f, (unsigned)d->loc.line,
             value(d->from, from), d->to ? d->to->name : "?");
}
void __ubsan_handle_float_cast_overflow_abort(struct fcast_data* d, uintptr_t from) { __ubsan_handle_float_cast_overflow(d, from); }
