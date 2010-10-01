PROG=iogen
VERSION_FILE=iogen_version.c
SOURCES=iogen.c $(VERSION_FILE)

CLPARSE_DIR=../clparse
CLPARSE_LIB=$(CLPARSE_DIR)/clparse.o
#CFLAGS=-g -Wall -static -I$(CLPARSE_DIR) -L$(CLPARSE_DIR)
CFLAGS=-g -Wall -I$(CLPARSE_DIR) -L$(CLPARSE_DIR)
LDFLAGS=-lclparse

VERSION:=$(shell git-describe HEAD &> /dev/null)
ifeq "$(VERSION)" ""
	VERSION:=$(shell git rev-parse HEAD)
endif

.PHONY: clean

$(PROG): $(SOURCES) $(CLPARSE_LIB)
	$(CC) $(CFLAGS) $^ -o $@

$(CLPARSE_LIB): FORCE
	$(MAKE) -C $(CLPARSE_DIR)

$(VERSION_FILE): FORCE
	@echo "const char *iogen_version = \""$(VERSION)"\";" > $@

FORCE:

clean:
	$(RM) $(VERSION_FILE) $(PROG) *~
