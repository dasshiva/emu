#ifndef __EMU_H__
#define __EMU_H__

#include <sys/types.h>
#include <stdio.h>

void error (char *fmt, ...);
#define SIZE 2097152
#define LOAD 0x200

typedef uint8_t mem;
typedef uint8_t u1;
typedef uint16_t u2;
typedef uint32_t u4;
typedef uint64_t u8;

#define MAJOR 0x0
#define MINOR 0x1

#define u2_conv_be(num) (num >> 8) | (num << 8)
#define u4_conv_be(num) (num >> 24) | (num << 8) | (num >> 8) | (num << 24)

void load(FILE* file, mem* memory);
#endif