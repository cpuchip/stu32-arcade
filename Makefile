# SPDX-License-Identifier: MIT
# stu32-arcade: Babal's rules in C (games/babal/core/), babalcore's tests (tests/), the oracle (oracle/).
CC ?= cc
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off  # the firmware's flags, and no fused multiply-adds
# games/babal/core/ and games/babal/app/ run on a device task's small stack: no frame above 1 KB (a 164 KB ctx once sat on one)
FRAME := -Wframe-larger-than=1024
BUILD := build
BABAL_SRC := $(wildcard games/babal/core/*.c)
BABAL_OBJ := $(BABAL_SRC:%.c=$(BUILD)/%.o)
CARGO ?= $(shell command -v cargo || echo $(HOME)/.cargo/bin/cargo)

.PHONY: all test oracle ref clean frames apphost appcheck driftcheck sdkcheck aimcheck landcheck snakecheck minescheck salvocheck

# The firmware's font and frame/app headers, read-only at FIRMWARE_PIN, exported with git archive
# from a local clone (FIRMWARE_REPO), never built in it.
FIRMWARE_REPO ?= ../stu32-firmware
FIRMWARE_PIN := $(shell cat FIRMWARE_PIN)
FW := $(BUILD)/firmware
FW_FILES := firmware/font5x7.c firmware/font5x7.h firmware/ls027.h firmware/apps.h firmware/keys.h \
    firmware/app_keys.h firmware/draw.h firmware/draw.c tests/draw_test.c
$(FW)/.pin: FIRMWARE_PIN
	rm -rf $(FW) && mkdir -p $(FW)
	git -C $(FIRMWARE_REPO) archive $(FIRMWARE_PIN) $(FW_FILES) | tar -x -C $(FW)
	echo $(FIRMWARE_PIN) > $@
APP_CFLAGS = -Igames/babal/app -Igames/babal/core -Ioracle -I$(FW)/firmware
# the app's own sources alone, under the frame guard (the host tools around them may use more)
$(BUILD)/app-frames.ok: games/babal/app/babal_app.c games/babal/app/render.c games/babal/app/babal_app.h games/babal/app/render.h $(FW)/.pin
	for f in games/babal/app/babal_app.c games/babal/app/render.c; do $(CC) $(CFLAGS) $(FRAME) $(APP_CFLAGS) -c $$f -o /dev/null || exit 1; done
	touch $@

$(BUILD)/frames: tools/frames.c games/babal/app/render.c games/babal/app/render.h $(ORACLE_SRC) oracle/oracle.h $(BUILD)/libbabal.a $(FW)/.pin
	$(CC) $(CFLAGS) $(APP_CFLAGS) tools/frames.c games/babal/app/render.c $(FW)/firmware/font5x7.c oracle/out.c oracle/game.c \
		$(BUILD)/libbabal.a -lm -o $@
frames: $(BUILD)/frames

APP_SRC := games/babal/app/babal_app.c games/babal/app/render.c
$(BUILD)/apphost: tools/apphost.c tools/render_ref.c $(APP_SRC) games/babal/app/babal_app.h games/babal/app/render.h $(ORACLE_SRC) oracle/oracle.h $(BUILD)/libbabal.a $(FW)/.pin
	$(CC) $(CFLAGS) $(APP_CFLAGS) tools/apphost.c tools/render_ref.c $(APP_SRC) $(FW)/firmware/font5x7.c oracle/out.c oracle/game.c \
		$(BUILD)/libbabal.a -lm -o $@
apphost: $(BUILD)/apphost $(BUILD)/app-frames.ok

# The app against babalcore, end to end, with plants (needs `make ref`).
appcheck: $(BUILD)/apphost ref
	oracle/app_check.sh

# The games SDK's drawing library: a per-pixel reference, line properties, plants.
sdkcheck: $(FW)/.pin
	oracle/sdk_check.sh

# AIM's five checks (docs/aim.md); with WASI_SDK, WAMRC and WAMR_TREE set, also as a .wasm through the bridge
aimcheck: $(FW)/.pin
	oracle/aim_check.sh

# LAND's checks: the logic against oracle/land, every pad landable, the landing rule's edges, plants
landcheck: $(FW)/.pin
	oracle/land_check.sh

# SNAKE's checks: the logic against oracle/snake, a full game to 880, the rules' edges, plants
snakecheck: $(FW)/.pin
	oracle/snake_check.sh

# MINES' checks: the logic against oracle/mines, no dealt board needing a guess (a complete solver), the edges, plants
minescheck: $(FW)/.pin
	oracle/mines_check.sh

# SALVO's checks: the logic against oracle/salvo (a model written from docs/salvo.md alone), long autopilot games,
# the edges, the app's frames, asks, save and sound (and the .wasm), plants
salvocheck: $(FW)/.pin
	oracle/salvo_check.sh

# Babal as a WebAssembly game against Babal native (needs WASI_SDK, WAMRC and WAMR_TREE; oracle/wasm_check.sh)
wasmcheck: $(FW)/.pin
	oracle/wasm_check.sh

# A struggling player's drift, the app's storage against storage that holds everything.
driftcheck: $(FW)/.pin
	oracle/drift_check.sh
all: $(BUILD)/babal_test $(BUILD)/driver

$(BUILD)/%.o: %.c games/babal/core/babal.h games/babal/core/internal.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(FRAME) -c $< -o $@

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
