#include <include/emu.h>
#define MAGIC 0xFACADE

#define check(file) \
  if (feof(file)) \
     error("Unexpected end of file");

static u1 read_u1(FILE* file) {
  check(file);
  u1 ret = 0;
  fread(&ret, sizeof(u1), 1, file);
  return ret;
}

static u2 read_u2(FILE* file) {
  check(file);
  u2 ret = 0;
  fread(&ret, sizeof(u2), 1, file);
  return ret;
}

static u4 read_u4(FILE* file) {
  check(file);
  u4 ret = 0;
  fread(&ret, sizeof(u4), 1, file);
  return ret; 
}

void load(FILE* file, mem* memory) {
  if (read_u4(file) != MAGIC)
    error("Magic number is invalid");
  if (read_u1(file) != MAJOR || read_u1(file) != MINOR)
    error("Unsupported version");
  u8 curr = ftell(file);
  fseek(file, 0, SEEK_END);
  u8 size = ftell(file);
  if ((size + LOAD) >= SIZE) 
    error("Program is too large");
  fseek(file, curr, SEEK_SET);
  fread(&memory[LOAD], sizeof(mem), size - curr + 1, file);
  fclose(file);
}