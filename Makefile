LEX = flex
YACC = bison

ifneq ($(RELEASE),)
 CFLAGS = -ansi -Wno-long-long -Wall -Wextra -pedantic -Wc++-compat -O2 -DNDEBUG
else
 CFLAGS = -ansi -Wno-long-long -Wall -Wextra -pedantic -Wc++-compat -ggdb3 -Og -fwrapv -Wshift-overflow=2
 YFLAGS = --debug
endif

all: generators kasm

generators: src/lexical.c src/lexical.h src/syntactic.c src/syntactic.h

src/lexical.c src/lexical.h: src/lexical.l src/syntactic.h
	$(LEX) --outfile=src/lexical.c --header-file=src/lexical.h $<

src/syntactic.c src/syntactic.h: src/syntactic.y
	$(YACC) --output=src/syntactic.c --header=src/syntactic.h $(YFLAGS) $<

assemblers:kasm

kasm: src/frontend_custom.c src/dictionary.c src/lexical.c src/semantic.c src/strcmpci.c src/syntactic.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ -o build/$@ $(LIBS)
