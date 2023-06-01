#ifndef __EMU_H__
#define __EMU_H__

#include <sys/types.h>
#include <stdio.h>

void error (char *fmt, ...);
#define SIZE 2097152

extern struct func** program;
typedef uint8_t mem;
typedef uint8_t u1;
typedef uint16_t u2;
typedef uint32_t u4;
typedef uint64_t u8;

struct func {
  char* name;
  u4 code;
};

void load(FILE* file, mem* memory);
void init(mem* memory);
void exec(void);

u1 read_u1(mem* memory, u4 offset);
u2 read_u2(mem* memory, u4 offset);
u4 read_u4(mem* memory, u4 offset);

typedef struct {
  u2 opcode : 6;
  u2 imm;
  u1 ra : 5;
  u1 rb : 5;
} i_t;

typedef struct {
  u2 opcode : 6;
  u2 opx : 11;
  u1 ra : 5;
  u1 rb : 5;
  u1 rc : 5;
} r_t;

typedef struct {
  u2 opcode : 6;
  u4 imm : 26;
} j_t;

extern j_t jtype;
extern r_t rtype;
extern i_t itype;

void decode_rtype(u4 ins);
void decode_itype(u4 ins);
void decode_jtype(u4 ins);
u4 sign_ext(u2 imm);
#endif