PROG=iogen
SOURCES=iogen.c

CLPARSE_DIR=../clparse
CLPARSE_LIB=$(CLPARSE_DIR)/clparse.o
CFLAGS=-g -Wall -static -I$(CLPARSE_DIR) -L$(CLPARSE_DIR)

.PHONY: clean

$(PROG): $(SOURCES) $(CLPARSE_LIB)
	$(CC) $(CFLAGS) $^ -o $@

$(CLPARSE_LIB):
	$(MAKE) -C $(CLPARSE_DIR)

clean:
	$(RM) $(PROG) *~
