import sys
import re

class File:
    def __init__(self, name):
        self.name = name
        self.file = open(name,'r')
        self.line = 0

    def readline(self):
            read = self.file.readline()
            if read == '':
              return "EOF"
            return read

    def gettok(self):
         line = self.readline()
         self.line += 1
         if line == 'EOF':
             return (line)
         split = list(map(lambda x: x.strip(), line.split(',')))
         if re.search(r"\s", split[0]):
           divide = split[0].split(' ')
           copy = []
           [copy.append(x) for x in divide if x != '']
           split[0] = copy[0]
           split.insert(1, copy[1])
         return split
     
file = sys.argv[1]
obj = File(file)
while True:
  read = obj.gettok()
  if isinstance(read, str):
    break
  print(read)