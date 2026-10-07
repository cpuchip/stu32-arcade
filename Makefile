# stu32-arcade: Babal's rules in C (babal/), babalcore's tests (tests/), the oracle (oracle/).
CC ?= cc
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off  # the firmware's flags, and no fused multiply-adds
BUILD := build
BABAL_SRC := $(wildcard babal/*.c)
BABAL_OBJ := $(BABAL_SRC:%.c=$(BUILD)/%.o)
CARGO ?= $(shell command -v cargo || echo $(HOME)/.cargo/bin/cargo)

.PHONY: all test oracle ref clean frames apphost appcheck driftcheck

# The firmware's font and frame/app headers, read-only at FIRMWARE_PIN, exported with git archive
# from a local clone (FIRMWARE_REPO), never built in it.
FIRMWARE_REPO ?= ../abacus-firmware
FIRMWARE_PIN := $(shell cat FIRMWARE_PIN)
FW := $(BUILD)/firmware
FW_FILES := firmware/font5x7.c firmware/font5x7.h firmware/ls027.h firmware/apps.h firmware/keys.h
$(FW)/.pin: FIRMWARE_PIN
	rm -rf $(FW) && mkdir -p $(FW)
	git -C $(FIRMWARE_REPO) archive $(FIRMWARE_PIN) $(FW_FILES) | tar -x -C $(FW)
	echo $(FIRMWARE_PIN) > $@
APP_CFLAGS = -Iapp -Ibabal -Ioracle -I$(FW)/firmware

$(BUILD)/frames: tools/frames.c app/render.c app/render.h $(ORACLE_SRC) oracle/oracle.h $(BUILD)/libbabal.a $(FW)/.pin
	$(CC) $(CFLAGS) $(APP_CFLAGS) tools/frames.c app/render.c $(FW)/firmware/font5x7.c oracle/out.c oracle/game.c \
		$(BUILD)/libbabal.a -lm -o $@
frames: $(BUILD)/frames

APP_SRC := app/babal_app.c app/render.c
$(BUILD)/apphost: tools/apphost.c tools/render_ref.c $(APP_SRC) app/babal_app.h app/render.h $(ORACLE_SRC) oracle/oracle.h $(BUILD)/libbabal.a $(FW)/.pin
	$(CC) $(CFLAGS) $(APP_CFLAGS) -DBABAL_CTX_LIMIT='(256 * 1024)' tools/apphost.c tools/render_ref.c $(APP_SRC) $(FW)/firmware/font5x7.c oracle/out.c oracle/game.c \
		$(BUILD)/libbabal.a -lm -o $@
apphost: $(BUILD)/apphost

# The app against babalcore, end to end, with plants (needs `make ref`).
appcheck: $(BUILD)/apphost ref
	oracle/app_check.sh

# A struggling player's drift, the app's storage against storage that holds everything.
driftcheck: $(FW)/.pin
	oracle/drift_check.sh
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
