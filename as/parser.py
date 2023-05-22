from enum import Enum

inst = ['addiu', 'sw', 'move', 'lw' 'nop', 'j']
class Token(Enum):
  LABEL = 1
  REGISTER = 2
  INS = 3
  NUM = 4
  OFFSET = 5

class Parser:
  def __init__(self, src):
    self.src = src
  
  def parse(self):
    while True:
      read = obj.gettok()
      if isinstance(read, str):
        break
      tk = self.tokenise(read)
  
  def instr(self, ins):
    for count, i in enumerate(inst) :
      if ins == i:
        return (True, count)
    return (False)

  def tokenise(self, tokens):
    if len(tokens) == 0:
      return []
    ret = []
    for i in tokens:
      if i[-1] == ':':
        ret.append(Token.LABEL)
        ret.append(i)
        continue
      elif i[0] == '$':
        if i[1].isnumeric():
          try:
            reg = int(i[1:])
            if reg > 31 or reg
      ins = self.instr(i)
      if ins[0] :
        ret.append(Token.INS)
        ret.append(ins[1])
        continue 
      try:
        num = int(i)
        ret.append(Token.NUMBER)
        ret.append(num)
        continue 
      except:
        pass
      