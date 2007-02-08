PROG=iogen
SOURCES=iogen.c

CFLAGS=-g -Wall -static -I../clparse -L../clparse
LIBS=../clparse/clparse.o

.PHONY: clean

$(PROG): $(SOURCES)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

clean:
	$(RM) $(PROG) *~
