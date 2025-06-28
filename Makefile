INCLUDE := -I.
CFLAGS := -std=c99 -Wall -Wextra -pedantic -MMD -MP $(INCLUDE)

CFLAGS += $(if $(ASAN),-fsanitize=address -g -O1)
LDFLAGS += $(if $(ASAN),-fsanitize=address)

DIRS := utils tests
SRC := $(wildcard $(addsuffix /*.c,$(DIRS)))
OBJ := $(patsubst %.c,build/%.o,$(SRC))

.PHONY: all test doc clean_doc clean

all: doc test

# Tests

test: build/tests/run_tests
	cd build/tests && ./run_tests || { rm run_tests; exit 1; }

build/tests/run_tests: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Documentation

doc: doc/main.pdf

clean_doc:
	rm -rf doc/utils

doc/main.pdf: doc/main.tex doc/utils/reg.h.tex
	cd doc && pdflatex $(notdir $<)

doc/%.tex: % scripts/c2tex.py
	mkdir -p $(dir $@)
	python3 scripts/c2tex.py $< > $@

# General

build/%.o: %.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p $(patsubst %,build/%,$(DIRS))

clean: clean_doc
	rm -rf build

-include $(OBJ:.o=.d)
