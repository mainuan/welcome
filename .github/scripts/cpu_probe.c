/* Executed by QEMU, never inferred from the host's /proc/cpuinfo. */
#include <cpuid.h>
#include <stdio.h>

int main(void)
{
    unsigned a, b, c, d;
    if (!__get_cpuid(1, &a, &b, &c, &d)) return 1;
    /* SSE3, SSSE3, FMA, CX16, SSE4.1/2, MOVBE, POPCNT, AVX, F16C. */
    const unsigned advanced = (1u << 0) | (1u << 9) | (1u << 12) | (1u << 13)
        | (1u << 19) | (1u << 20) | (1u << 22) | (1u << 23) | (1u << 28) | (1u << 29);
    if ((c & advanced) || !(d & (1u << 26))) return 2;
    if (__get_cpuid(0x80000001, &a, &b, &c, &d) && (c & ((1u << 0) | (1u << 5)))) return 3;
    if (__get_cpuid_count(7, 0, &a, &b, &c, &d) && (b & ((1u << 3) | (1u << 5) | (1u << 8)))) return 4;
    puts("Guest CPUID confirmed: SSE2, no x86-64-v2/v3 feature set (no AVX/AVX2).");
    return 0;
}
