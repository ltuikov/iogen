PROG=iogen
SOURCES=iogen.c

CFLAGS=-g -Wall -static -I../clparse -L../clparse
CLPARSE_DIR=../clparse
CLPARSE_LIB=$(CLPARSE_DIR)/clparse.o

.PHONY: clean

$(PROG): $(SOURCES) $(CLPARSE_LIB)
	$(CC) $(CFLAGS) $^ -o $@

$(CLPARSE_LIB):
	$(MAKE) -C ../clparse

clean:
	$(RM) $(PROG) *~
