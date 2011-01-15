#
# Versatile Threaded I/O generator
# Copyright (C) 2006-2011 Luben Tuikov
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

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
