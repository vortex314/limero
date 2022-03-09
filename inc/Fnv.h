#ifndef _FNV_H_
#define _FNV_H_
#include <stdint.h>

#ifndef FNV_LENGTH
#define FNV_LENGTH 16
#endif

#if FNV_LENGTH == 16
#define FNV_PRIME 16777619
#define FNV_OFFSET 2166136261
#define FNV_MASK 0xFFFF
#endif

#if FNV_LENGTH == 32
#define FNV_PRIME 16777619
#define FNV_OFFSET 2166136261
#define FNV_MASK 0xFFFFFFFFu
#endif

#if FNV_LENGTH == 64
#define FNV_PRIME 1099511628211ull
#define FNV_OFFSET 14695981039346656037ull
#endif

constexpr uint32_t fnv1(uint32_t h, const char *s)
{
    return (*s == 0) ? h
           : fnv1((h * FNV_PRIME) ^ static_cast<uint32_t>(*s), s + 1);
}

constexpr uint16_t H(const char *s)
{
    //    uint32_t  h = fnv1(FNV_OFFSET, s) ;
    return (fnv1(FNV_OFFSET, s) & FNV_MASK);
}
#endif