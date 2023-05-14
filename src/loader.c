#include <include/elf.h>
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

void valid_elf_magic(FILE* file) {
  if (read_u1(file) != 0x7F)
    goto err;
  if (read_u1(file) != 45)
    goto err;
  if (read_u1(file) != 0x4c)
    goto err;
  if (read_u1(file) != 46)
    goto err;
err:
  error("Not an ELF executable!");
}

void load(FILE* file, mem* memory) {
  valid_elf_magic(file);
  if (read_u1(file) != ELFCLASS32)
    error("File is not a 32 bit");
  if (read_u1(file) != ELFDATA2LSB)
    error("File is not little endian");
  if (read_u1(file) != EV_CURRENT)
    error("Invalid elf file version");
  read_u4(file);
  read_u4(file);
  read_u1(file);
  if (read_u2(file) != ET_EXEC)
    error("File not marked executable");
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