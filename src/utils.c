#include <emu.h>
#include <limits.h>
u1 read_u1(mem* memory, u8 offset) {
  if (offset > SIZE)
    error("Addressing memoryory beyond 2MB");
  return memory[offset];
}

u2 read_u2(mem* memory, u8 offset) {
  u1 read1 = read_u1(memory, offset);
  u1 read2 = read_u1(memory, offset + 1);
  return (read2 << 8) | read1;
}

u4 read_u4(mem* memory, u8 offset) {
  u1 read1 = read_u1(memory, offset);
  u1 read2 = read_u1(memory, offset + 1);
  u1 read3 = read_u1(memory, offset + 2);
  u1 read4 = read_u1(memory, offset + 3);
  return (read4 << 24) | (read3<< 16) | (read2 << 8) | read1;
}

void print_bin(u4 integer)
{
    int i = CHAR_BIT * sizeof integer;
    while(i--) {
        putchar('0' + ((integer >> i) & 1)); 
    }
}