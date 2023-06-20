#include <emu.h>

u1 read_u1(mem* memory, u4 offset) {
  if (offset > SIZE)
    error("Addressing memory beyond 2MB");
  return memory[offset];
}

u2 read_u2(mem* memory, u4 offset) {
  u1 read1 = read_u1(memory, offset);
  u1 read2 = read_u1(memory, offset + 1);
  return (read2 << 8) | read1;
}

u4 read_u4(mem* memory, u4 offset) {
  u1 read1 = read_u1(memory, offset);
  u1 read2 = read_u1(memory, offset + 1);
  u1 read3 = read_u1(memory, offset + 2);
  u1 read4 = read_u1(memory, offset + 3);
  return (read4 << 24) | (read3 << 16) | (read2 << 8) | read1;
}

instr ins;

void decode(mem* memory, u4 offset) {
  ins.format = read_u1(memory, offset++);
  ins.dest = read_u1(memory, offset++);
  switch (ins.format) {
    case 0: ins.regarg = read_u1(memory, offset++); break;
    case 1: ins.imm = read_u4(memory, offset); offset += 4; break;
    case 2: ins.offset = read_u4(memory, offset); offset += 4; break;
    default: error("Unknown format bit of instruction %d", ins.format);
  }
}

u4 sign_ext(u2 imm) {
  if ((imm & (1 << 15)) != 0) {
    return -((~imm & 0xFFFF)+ 1);
  }
  return 0xFFFF & imm;
}