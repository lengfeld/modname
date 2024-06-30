# SPDX-License-Identifier: GPL-2.0-only
# SPDX-FileCopyrightText: 2022 Stefan Lengfeld

CFLAGS = -Wall -Werror -pedantic-errors -std=c11
LDFLAGS = -lreadline

TARGETS = modname
ALL_TARGETS = ${TARGETS} modnametest

all: $(TARGETS)

modname: modname.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

modnametest: modname.c
	$(CC) $(CFLAGS) -DTEST -o $@ $< $(LDFLAGS)

%.html: %.md
	pandoc -s $< > $@

.PHONY: format
format:
	clang-format -i modname.c

.PHONY: test
test: $(TARGETS)
	./test.py

.PHONY: lint
lint:
	pycodestyle test.py

.PHONY: clean
clean:
	rm -f ${ALL_TARGETS}
