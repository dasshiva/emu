#include <stdarg.h>
#include <stdlib.h>
#include <stdarg.h>
#include <include/emu.h>

__attribute__((noreturn)) void error (char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  puts ("");
  exit (1);
}

static void prepare(mem* memory) {
  u4 offset = 0;
  if (read_u4(memory, offset) != 0xFACADE)
    error("Magic number is invalid");
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
  prepare(memory);
  init(memory);
  //exec();
  return 0;
}
