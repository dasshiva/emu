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

// these will be reused as needed
r_t rtype;
i_t itype;
j_t jtype;

void decode_rtype(u4 ins) {
  rtype.opcode = ins & 63;
  ins >>= 6;
  rtype.opx = ins & 2047;
  ins >>= 11;
  rtype.rc = ins & 31;
  ins >>= 5;
  rtype.rb = ins & 31;
  ins >>= 5;
  rtype.ra =  ins;
}

void decode_itype(u4 ins) {
  itype.opcode = ins & 63;
  ins >>= 6;
  itype.imm = ins & 0xFFFF;
  ins >>= 16;
  itype.rb = ins & 31;
  ins >>= 5;
  itype.ra = ins;
}

void decode_jtype(u4 ins) {
  jtype.opcode = ins & 63;
  ins >>= 6;
  jtype.imm = ins;
}

u4 sign_ext(u2 imm) {
  if ((imm & (1 << 15)) != 0) {
    return -((~imm & 0xFFFF)+ 1);
  }
  return 0xFFFF & imm;
}