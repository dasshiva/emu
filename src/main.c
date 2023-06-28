#include <stdarg.h>
#include <stdlib.h>
#include <stdarg.h>
#include <include/emu.h>

struct func** program;
u2 psize;

__attribute__((noreturn)) void error (char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  puts ("");
  exit (1);
}

static u4 prepare(mem* memory) {
  u4 ret = 0;
  u4 offset = 0;
  if (read_u4(memory, offset) != MAGIC)
    error("Magic number is invalid");
  offset += 4;
  if (read_u2(memory, offset) != CKSUM)
    error("Unsupported version");
  offset += 2;
  psize = read_u2(memory, offset);
  if (psize < 1) 
    error("File has no functions");
  offset += 2;
  program = malloc (sizeof (struct func*) * psize);
  for (u2 ind = 0; ind < psize; ind++) {
    printf(" %d", offset);
    program[ind] = malloc (sizeof (struct func));
    u2 len = read_u2(memory, offset);
    if (len < 1)
      error("Invalid function/data name at offset %d", offset);
    program[ind]->name = malloc(sizeof (char) * len + 1);
    offset += 2;
    for (u2 len2 = 0; len2 < len; len2++, offset++) {
      program[ind]->name[len2] = read_u1(memory, offset);
    }
    program[ind]->name[len] = '\0';
    if (read_u1(memory, offset) == 1) {
      program[ind]->ty = 1;
      offset++;
      u1 width = read_u1(memory, offset++);
      switch (width) {
        case 1: program[ind]->data = read_u1(memory, offset++); break;
        case 2: program[ind]->data = read_u2(memory, offset);
        offset += 2;
        break;
        case 4: program[ind]->data = read_u4(memory, offset);
        offset += 4;
        break;
      }
      continue;
    }
    offset++;
    u4 code = read_u4(memory, offset);
    offset += 4;
    program[ind]->code = offset;
    if (strcmp(program[ind]->name, "_start") == 0) 
      ret = offset;
    offset += code;
  }
  return ret;
}


int main (int argc, char *argv[]) {
  if (argc < 2)
    error ("Need a filename");
  FILE *file = fopen(argv[1], "r");
  if (!file)
    error ("File %s could not be opened", argv[1]);
  mem* memory = (mem*) malloc(sizeof(mem) * SIZE);
  if (!memory) {
    error("Memory could not be allocated");
  }
  u8 curr = ftell(file);
  fseek(file, 0, SEEK_END);
  u8 size = ftell(file);
  if (size >= SIZE) 
    error("Program is too large");
  fseek(file, curr, SEEK_SET);
  fread(memory, sizeof(mem), size - curr + 1, file);
  fclose(file);
  u4 startpc = prepare(memory);
  init(memory, startpc);
  exec();
  return 0;
}
