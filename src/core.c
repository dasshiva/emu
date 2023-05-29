#include <include/emu.h>

typedef struct {
  u4 regs[32];
  u4 cregs[16];
  mem* memory;
  u8 pc;
} core;

static core main;

void set_reg(u2 reg, u4 val) {
  if (reg == 0)
    return;
  main.regs[reg] = val;
}
void init(mem* memory) {
  main.memory = memory;
  main.pc = 0;
  main.cregs[0] = (1 << 0); // in superviser mode
  main.cregs[5] = 0xFACADE; // identify ourselves 
  main.cregs[11] = (1 << 0); // we have an mpu but disabled on start
}

void exec(void) {
  while (1) {
    u4 ins = read_u4(main.memory, main.pc);
    main.pc += 4;
    u4 opcode = ins & 63;
    switch (opcode) {
      case 0x04: {
        decode_itype(ins);
        set_reg(itype.rb, main.regs[itype.ra] + sign_ext(itype.imm));
        break;
      }
      case 0x3A: {
        
      }
    }
    break;
  }
}