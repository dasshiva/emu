#include <include/emu.h>

typedef struct {
  u8 regs[32];
  //u4 cregs[16];
  mem* memory;
  u8 pc;
} core;

static core main;

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
    u1 opcode = read_u1(main.memory, main.pc++);
    decode(main.memory, main.pc);
    main.pc += 7;
    fprintf(stderr, "%d ", opcode);
    switch (opcode) {
      case 8: return;
      case 11: {
        fprintf(stderr, "%d ", ins.format);
        if (ins.format == 0)
          main.regs[ins.dest] = main.regs[ins.regarg];
        else if (ins.format == 1)
          main.regs[ins.dest] = ins.imm;
        break;
      }
      case 13: return;
      default: error("Unknown opcode: %d", opcode);
    }
  }
}