############################################################################
# tools/preview/Makefile
#
# Render the desktop pages on the host into out/*.png with the LVGL tree
# and LVGL configuration of the board.
#
#   make LVGL_DIR=<checkout of open-vela/apps_graphics_lvgl>
#   make LVGL_DIR=... run         # write out/*.png
#   ./compare.py                  # side by side with docs/launcher-v2/images
#
# lvdemo also needs DEMOS_DIR, a copy of openvela packages/demos with
# snake_game and Whackmole.
############################################################################

LVGL_DIR ?= $(error LVGL_DIR must point to open-vela/apps_graphics_lvgl)
DEMOS_DIR ?= $(error DEMOS_DIR must point to openvela packages/demos)

APP     = ../../app
OUT     = build
SCENES  = launcher paint bench monitor touch lvdemo

CFLAGS  = -O2 -g -Wall -Wno-unused-parameter
CFLAGS += -I$(OUT)/inc -Istub -I. -I$(APP)/ui -I$(APP)/digit
CFLAGS += -DLV_CONF_KCONFIG_EXTERNAL_INCLUDE='"lv_kconfig.h"'

LVGL_SRCS = $(shell find $(LVGL_DIR)/src $(LVGL_DIR)/demos -name '*.c')
LVGL_OBJS = $(patsubst $(LVGL_DIR)/%.c,$(OUT)/lvgl/%.o,$(LVGL_SRCS))

UI_FONTS = $(wildcard $(APP)/ui/fonts/*.c)
UI_SRCS = $(APP)/ui/lvx_ui.c $(UI_FONTS)

SRCS_launcher = $(APP)/launcher/launcher_main.c
SRCS_paint    = $(APP)/paint/paint_main.c $(APP)/digit/digit.c \
                $(APP)/digit/digit_model.c
SRCS_bench    = $(APP)/bench/bench_main.c $(APP)/digit/digit.c \
                $(APP)/digit/digit_model.c $(APP)/digit/digit_samples.c
SRCS_monitor  = $(APP)/monitor/monitor_main.c
SRCS_touch    = $(APP)/touch/touch_main.c
SRCS_lvdemo   = $(APP)/lvdemo/lvdemo_main.c $(APP)/lvdemo/lvdemo_snake.c \
                $(APP)/lvdemo/lvdemo_whackmole.c

$(OUT)/preview_lvdemo: CFLAGS += -I$(DEMOS_DIR)/snake_game \
  -I$(DEMOS_DIR)/Whackmole -I$(DEMOS_DIR)/Whackmole/pic -I$(OUT)/inc/lvgl

# Home tile layouts with 0 to 5 pinned apps

LAYOUTS = 0 1 2 3 4 5
APPS_0  =
APPS_1  = paint
APPS_2  = paint bench
APPS_3  = paint bench monitor
APPS_4  = paint bench monitor touch
APPS_5  = paint bench monitor touch lvgl_widgets

all: $(SCENES:%=$(OUT)/preview_%) $(LAYOUTS:%=$(OUT)/layout_%)

$(OUT)/inc/lvgl:
	mkdir -p $(OUT)/inc
	ln -sfn $(abspath $(LVGL_DIR)) $@

$(OUT)/lvgl/%.o: $(LVGL_DIR)/%.c | $(OUT)/inc/lvgl
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -w -c -o $@ $<

$(OUT)/liblvgl.a: $(LVGL_OBJS)
	$(AR) rcs $@ $^

.SECONDEXPANSION:
$(OUT)/preview_%: scene_%.c preview.c preview.h $(UI_SRCS) $$(SRCS_$$*) \
                  $(OUT)/liblvgl.a
	$(CC) $(CFLAGS) -include preview.h -o $@ scene_$*.c preview.c \
	  $(filter-out %_main.c,$(SRCS_$*)) $(UI_SRCS) $(OUT)/liblvgl.a -lz -lm

$(OUT)/layout_%: scene_launcher.c preview.c preview.h $(UI_SRCS) \
                 $(SRCS_launcher) $(OUT)/liblvgl.a
	$(CC) $(CFLAGS) -include preview.h -DPREVIEW_LAYOUT=$* \
	  -DCONFIG_LVX_LAUNCHER_PINNED='"$(APPS_$*)"' -o $@ scene_launcher.c \
	  preview.c $(UI_SRCS) $(OUT)/liblvgl.a -lz -lm

run: all
	mkdir -p out
	for s in $(SCENES); do $(OUT)/preview_$$s || exit 1; done
	for l in $(LAYOUTS); do $(OUT)/layout_$$l || exit 1; done

clean:
	rm -rf $(OUT) out

.PHONY: all run clean
