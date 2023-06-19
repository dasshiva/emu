from enum import Enum
from fixedint import *
import copy
class Scanner:
  def __init__(self, file):
    self.file = file
    self.handle = open(file, "r")
    self.line = 1
    self.text = ""
  
  def change_comma(self):
    new = ""
    for i in self.text:
      if i == ',':
        continue
      new += i
    self.text = new
    
  def next(self):
    self.text = self.handle.readline()
    if self.text == "":
      return ["EOF"]
    self.change_comma()
    return self.text.split()
    
  def error(self, msg):
    print(f"In {self.file} at line {self.line} : {self.text}\nError: {msg}")
    import sys
    sys.exit(1)
# Entries in this table have the following format:
# Instruction Arguments Opcode Instruction-Format where R represents register, I represents immediate. So for example RRI means two register argument followed by an immediate 
insn = [("add", 2, 1, 'RR/RI'),
        ("sub", 2, 2, 'RR/RI'),
        ("mul", 2, 3, 'RR/IR'),
        ("div", 2, 4, 'RR/IR'),
        ("and", 2, 5, 'RR/IR'),
        ("or", 2, 6, 'RR/IR'),
        ("xor", 2, 7, 'RR/IR'),
        ("panic", 0, 8, 'N'),
        ("push", 1, 9, 'R'),
        ("pop", 1, 10, 'R'),
        ("mov", 0, 1, 'S')
       ]

data_attr = ["byte", "short", "int"]
class Token (Enum):
  COMMA = 1
  INSN = 2
  NUMBER = 3
  LABEL = 4
  REG = 5
  DIRECT = 6
  
class Parser:
  def __init__(self, scan):
    self.src = scan
    self.tokens = []
    self.symtab = []
  
  def tokenise(self, text):
    for token in text:
      if token[0].isnumeric() or token[0] == '$':
        if token[0] == '$':
          token = token[1:]
        try:
          self.tokens.append((Token.NUMBER, int(token)))
        except:
          self.src.error(f"Malformed number {token}")
      elif token[0] == 'x':
        try:
          reg = int(token[1:])
          if reg >= 31 or reg <= 0:
            raise Exception("")
          self.tokens.append(( Token.REG, reg ))
        except:
          self.src.error(f"Unknown register {token}")
      elif token[0] == '.':
        if token[-1] == ':':
          self.tokens.append(( Token.LABEL, token ))
        else:
          self.tokens.append(( Token.ATTR[1:] , token ))
      else:
        if token[-1] == ':':
          self.tokens.append(( Token.LABEL, token[:-1] ))
        elif token == 'sp':
          self.tokens.append(( Token.REG, 26 ))
        else:
          self.tokens.append(( Token.INSN, token ))
  
  def parse_with(self, form, parsed):
    parse = []
    for i, c in enumerate(form, 1):
      if c == 'R' && self.tokens[i][0] != TOKEN.REG:
        parse.clear()
        return False
      elif c == 'I' && self.tokens[i][0] != TOKEN.INT:
        parse.clear()
        return False
      else:
        parse.append(self.tokens[i][1])
    parsed += parse
    return True
    
  def parse_insn(self):
    parsed = []
    index = -1
    try:
      index = insn.index(self.tokens[0][0])
    except:
      self.src.error(f"Instruction {self.tokens[0][1]} does not exist")
    ins = insn[index]
    parsed.append(ins[2])
    if ins[3].find('/'):
      parts = ins[3].split('/')
      for i in parts:
        if self.parse_with(i, parsed):
          return parsed
      self.src.error(f"Instruction {self.tokens[0][1]} has been given invalid or incorrect number of arguments")
    else:
      

  def parse(self):
    while True:
      line = sc.next()
      if line[0] != "EOF":
        self.tokenise(line)
        for tok in self.tokens:
          if tok[0] == Token.INSN:
            parsed = self.parse_insn()
            if len(self.symtab) == 0:
              self.src.error("Instructions are not allowed at top  level")
            self.symtab[-1].append(parsed)
            print(self.symtab)
            self.tokens.clear()
            break
          elif tok[0] == Token.LABEL:
            self.symtab.append([tok[1]])
            self.tokens.clear()
            break
          elif tok[0] == Token.ATTR:
            for count, attr in enumerate(data_attr):
              if attr == tok[1]:
                self.symtab.append("__data__" + [tok[1]])
                if len(self.tokens) == 2:
                  self.symtab[-1].append([count, 0])
                else:
                  self.symtab[-1].append([count, self.tokens[2]])
                break
          else:
            self.src.error("Only directives and labels are allowed at top level")
      else:
        break

  def codegen(self):
    for func in self.symtab:
      opcode = MutableUInt32(0)
      for i, ins in enumerate(func):
        if i == 0:
          continue
        ref = insn[ins[0] - 1]
        if ref[3] == 'RRI':
          opcode = ins[3]
          opcode <<= 16;
          opcode |= ins[2] << 11
          opcode |= ins[1] << 6
          opcode |= ins[0]
        elif ref[3] == 'N':
          opcode = ins[0]
        func[i] = copy.deepcopy(opcode)
    out = open('hello.out', 'wb')
    out.write(0xFACADE.to_bytes(4, 'little'))
    out.write(0xFCA.to_bytes(2, 'little'))
    out.write(len(self.symtab).to_bytes(2, 'little'))
    def strip(string):
      if string.startswith('__data__'):
        return string[len('__data__'):]
      return string
    for func in self.symtab:
      out.write(len(strip(func[0])).to_bytes(2, 'little'))
      out.write(bytes(func[0], 'ascii'))
      if func[0].startswith("__data__"):
        out.write(0x1.to_bytes(1, 'little'))
        width = func[1][0]
        out.write(width.to_bytes(1, 'little'))
        out.write(func[1][1].to_bytes(width, 'little'))
      else:
        out.write(0x0.to_bytes(1, 'little'))
        out.write(((len(func) - 1) * 4).to_bytes(4, 'little'))
        for opcode in func[1:]:
          out.write(opcode.to_bytes(4, 'little'))
          
sc = Scanner("../hello.s")
p = Parser(sc)
p.parse()
p.codegen()
 