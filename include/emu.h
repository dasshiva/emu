#ifndef __EMU_H__
#define __EMU_H__

#include <sys/types.h>
#include <stdio.h>

void error (char *fmt, ...);
#define SIZE 2097152
#define MAGIC 0xFACADE
#define CKSUM 0xFCA

typedef uint8_t mem;
typedef uint8_t u1;
typedef uint16_t u2;
typedef uint32_t u4;
typedef uint64_t u8;

struct func {
  char* name;
  u1 ty;
  u4 code;
  u4 data;
};

extern struct func** program;
extern u2 psize;
void load(FILE* file, mem* memory);
void init(mem* memory, u4 startpc);
void exec(void);

typedef struct {
  u1 format;
  u1 dest;
  u1 regarg;
  u4 imm;
  u4 offset;
} instr;

extern instr ins;

u1 read_u1(mem* memory, u4 offset);
u2 read_u2(mem* memory, u4 offset);
u4 read_u4(mem* memory, u4 offset);

u4 sign_ext(u2 imm);
#endif