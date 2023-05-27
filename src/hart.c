#include <include/emu.h>

typedef struct {
  u8 regs[32];
  mem* memory;
  u8 pc;
} hart;

static hart main;

void init(mem* memory) {
  main.memory = memory;
  main.pc = 0;
}

void exec(void) {
  while (1) {
    u4 ins = read_u4(main.memory, main.pc);
    main.pc += 4;
    //print_bin(ins & 0x7F);
    u4 opcode = ins & 0x7F;
    switch (opcode) {
      case 0b110011: {
        
      }
    }
    break;
  }
}