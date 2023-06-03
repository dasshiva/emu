import sys
from enum import Enum

if len(sys.argv) < 1:
  print('Need a filename')
file = open(sys.argv[1], 'r').read()

class Token(Enum):
  SOF = 1
  NUM = 2
  ID = 3
  COMMA = 4
  STR = 5
  CHAR = 6
  OP_BR = 7
  CL_BR = 8
  OP_SQBR = 9
  CL_SQBR = 10
  OP_CUBR = 11
  CL_CUBR = 12
  DEREF = 13
  BIT_OR = 14
  LOG_OR = 15
  LOG_AND = 16
  LOG_NOT = 17
  LT = 18
  GT = 19
  LE = 20
  GE = 21
  LSHIFT = 22
  RSHIFT = 23
	DOT = 24
	ARROW = 25
	PLUS = 26
	MINUS = 27
	MINUSEQ = 28
	PLUSEQ = 29
	OREQ = 30
	ANDEQ = 31
	EQ = 32
	NEQ = 33
	ASSIGN = 34
	INC = 35
	DEC = 36
	COLON = 37
	SEMICOL = 38
	EOF = 39
	AMP = 49
	RET = 41
	IF = 42
	ELSE = 43
	WHILE = 44
	FOR = 45
	DO = 46
	OP_COMM = 47
	CL_COMM = 48
	DEFINE = 49
	INCLUDE = 50
	TYPEDEF = 51
	ENUM = 52
	STRUCT = 53
	SZOF = 54
	ELLIP = 55
	ASM = 56
	SWITCH = 57
	CASE = 58
	BREAK = 59
	DEF = 60
# Nice


  