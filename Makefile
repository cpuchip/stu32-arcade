# stu32-arcade: Babal's rules in C (babal/), babalcore's tests (tests/), the oracle (oracle/).
CC ?= cc
CFLAGS ?= -O2
CFLAGS += -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -ffp-contract=off
BUILD := build
BABAL_SRC := $(wildcard babal/*.c)
BABAL_OBJ := $(BABAL_SRC:%.c=$(BUILD)/%.o)
CARGO ?= $(shell command -v cargo || echo $(HOME)/.cargo/bin/cargo)

.PHONY: all test oracle ref clean
all: $(BUILD)/babal_test $(BUILD)/driver

$(BUILD)/%.o: %.c babal/babal.h babal/internal.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libbabal.a: $(BABAL_OBJ)
	ar rcs $@ $^

$(BUILD)/babal_test: tests/babal_test.c $(BUILD)/libbabal.a
	$(CC) $(CFLAGS) $< $(BUILD)/libbabal.a -lm -o $@

ORACLE_SRC := oracle/driver.c oracle/out.c oracle/game.c
$(BUILD)/driver: $(ORACLE_SRC) oracle/oracle.h $(BUILD)/libbabal.a
	$(CC) $(CFLAGS) $(ORACLE_SRC) $(BUILD)/libbabal.a -lm -o $@

test: $(BUILD)/babal_test
	$(BUILD)/babal_test

# The reference: babalcore at the pinned commit, never edited (oracle/fetch-babal.sh).
ref:
	oracle/fetch-babal.sh
	cd oracle/ref && $(CARGO) build --release --locked -q

oracle: all ref
	oracle/run.sh

clean:
	rm -rf $(BUILD)
