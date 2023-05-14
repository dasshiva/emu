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

int
main (int argc, char *argv[])
{
  if (argc < 2)
    error ("Need a filename");
  FILE *file = fopen(argv[1], "r");
  if (!file)
    error ("File %s could not be opened", argv[1]);
  mem* memory = (mem*) malloc(sizeof(mem) * SIZE);
  if (!memory) {
    error("Memory could not be allocated");
  }
  load(file, memory);
  return 0;
}
