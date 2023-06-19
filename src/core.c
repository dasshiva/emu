#include <include/emu.h>

typedef struct {
  u8 regs[32];
  //u4 cregs[16];
  mem* memory;
  u8 pc;
} core;

static core main;

void set_reg(u2 reg, u4 val) {
  if (reg == 0)
    return;
  main.regs[reg] = val;
}

void init(mem* memory, u4 startpc) {
  main.memory = memory;
  main.pc = startpc;
 /* main.cregs[0] = (1 << 0); // in superviser mode
  main.cregs[5] = 0xFACADE; // identify ourselves 
  main.cregs[11] = (1 << 0); // we have an mpu but disabled on start */
}

char* find_function() {
  for (u2 i = 0; i < psize; i++) {
    if (program[i]->code < main.pc) {
      if (i + 1 == psize)
      return program[i]->name;
      if (program[i + 1]->code > main.pc)
      return program[i]->name;
    }
  }
}
void exec(void) {
  while (1) {
    u4 ins = read_u4(main.memory, main.pc);
    main.pc += 4;
    u4 opcode = ins & 63;
    switch (opcode) {
      default: error("Unknown opcode: %d", opcode);
    }
    break;
  }
}