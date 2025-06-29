INCLUDE := -I.
CFLAGS := -std=c99 -Wall -Wextra -Werror -pedantic -MMD -MP $(INCLUDE)

CFLAGS += $(if $(FANALYZER),-fanalyzer)
CFLAGS += $(if $(ASAN),-fsanitize=address -g -O1)
LDFLAGS += $(if $(ASAN),-fsanitize=address)

BUILD := build
PROG := tests/run_tests
DIRS := utils tests
SRC := $(wildcard $(addsuffix /*.c,$(DIRS)))
OBJ := $(patsubst %.c,$(BUILD)/%.o,$(SRC))

.PHONY: all test doc clean_doc format check clean

all: $(patsubst %,$(BUILD)/%,$(PROG))

# Programs

$(BUILD)/tests/run_tests: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Documentation

doc: doc/main.pdf

clean_doc:
	rm -rf doc/utils

doc/main.pdf: doc/main.tex doc/utils/reg.h.tex
	cd doc && pdflatex $(notdir $<)

doc/%.tex: % scripts/c2tex.py
	mkdir -p $(dir $@)
	echo -n '\\' > $@
	git log -n 1 --pretty=format:"commit{$<}{%ad}{%H}" -- $< >> $@
	python3 scripts/c2tex.py $< >> $@

# Testing and linting

EXCLUDE := utils/snprintf.c utils/snprintf.h
CHECK := $(filter-out $(EXCLUDE),$(wildcard $(addsuffix /*.[ch],$(DIRS))))

check: format cppcheck tidy test

format:
	clang-format --dry-run -Werror $(CHECK)

cppcheck:
	perl scripts/colorize.pl --enable=all --inconclusive \
		--std=c99 --force --quiet --inline-suppr --error-exitcode=1 \
		--suppress=missingInclude $(CHECK)

tidy: | $(BUILD)
	$(MAKE) clean
	intercept-build-14 --cdb $(BUILD)/compile_commands.json $(MAKE) all
	clang-tidy $(CHECK) -p $(BUILD) -system-headers -warnings-as-errors=*

test: all
	cd $(BUILD)/tests && ./run_tests || { rm run_tests; exit 1; }

# General

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(patsubst %,$(BUILD)/%,$(DIRS))

clean: clean_doc
	rm -rf $(BUILD)

-include $(OBJ:.o=.d)
