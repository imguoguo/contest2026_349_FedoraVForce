############################################################################
# tools/preview/Makefile
#
# Render the desktop pages on the host into out/*.png with the LVGL tree
# and LVGL configuration of the board.
#
#   make LVGL_DIR=<checkout of open-vela/apps_graphics_lvgl> \
#        FONT_DIR=<dir with Oswald[wght].ttf and Outfit[wght].ttf>
#   make ... run                  # write out/*.png
#   ./compare.py                  # side by side with docs/launcher-v2/images
############################################################################

LVGL_DIR ?= $(error LVGL_DIR must point to open-vela/apps_graphics_lvgl)

APP     = ../../app
OUT     = build
SCENES  = launcher paint bench monitor

CFLAGS  = -O2 -g -Wall -Wno-unused-parameter
CFLAGS += -I$(OUT)/inc -Istub -I. -I$(APP)/ui
CFLAGS += -DLV_CONF_KCONFIG_EXTERNAL_INCLUDE='"lv_kconfig.h"'

LVGL_SRCS = $(shell find $(LVGL_DIR)/src -name '*.c')
LVGL_OBJS = $(patsubst $(LVGL_DIR)/%.c,$(OUT)/lvgl/%.o,$(LVGL_SRCS))

# Until app/ui/fonts is generated, ttf2lv.py makes stand-ins from the
# Oswald and Outfit variable fonts of google/fonts in FONT_DIR

UI_FONTS = $(wildcard $(APP)/ui/fonts/*.c)
ifeq ($(UI_FONTS),)
FONT_DIR ?= $(error FONT_DIR must hold Oswald[wght].ttf and Outfit[wght].ttf)
UI_FONTS = $(OUT)/fonts/ui_font_oswald_300.c $(OUT)/fonts/ui_font_oswald_120.c \
           $(OUT)/fonts/ui_font_oswald_64.c $(OUT)/fonts/ui_font_outfit_26.c \
           $(OUT)/fonts/ui_font_outfit_34.c $(OUT)/fonts/ui_font_outfit_52.c
CFLAGS  += -DUI_HAVE_FONTS
endif

UI_SRCS = $(APP)/ui/lvx_ui.c $(UI_FONTS)

SRCS_launcher = $(APP)/launcher/launcher_main.c
SRCS_paint    = $(APP)/paint/paint_main.c $(APP)/paint/digit.c \
                $(APP)/paint/digit_model.c
SRCS_bench    = $(APP)/bench/bench_main.c
SRCS_monitor  = $(APP)/monitor/monitor_main.c

# Home tile layouts with 0 to 5 apps, "clock" and "camera" are dummies

LAYOUTS = 0 1 2 3 4 5
APPS_0  =
APPS_1  = paint
APPS_2  = paint bench
APPS_3  = paint bench monitor
APPS_4  = paint bench monitor clock
APPS_5  = paint bench monitor clock camera

all: $(SCENES:%=$(OUT)/preview_%) $(LAYOUTS:%=$(OUT)/layout_%)

$(OUT)/inc/lvgl:
	mkdir -p $(OUT)/inc
	ln -sfn $(abspath $(LVGL_DIR)) $@

$(OUT)/lvgl/%.o: $(LVGL_DIR)/%.c | $(OUT)/inc/lvgl
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -w -c -o $@ $<

$(OUT)/fonts/stamp: ttf2lv.py
	mkdir -p $(OUT)/fonts
	python3 -I ttf2lv.py "$(FONT_DIR)/Oswald[wght].ttf" \
	  "$(FONT_DIR)/Outfit[wght].ttf" $(OUT)/fonts
	touch $@

$(OUT)/fonts/%.c: $(OUT)/fonts/stamp
	@true

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
	  -DCONFIG_LVX_LAUNCHER_APPS='"$(APPS_$*)"' -o $@ scene_launcher.c \
	  preview.c $(UI_SRCS) $(OUT)/liblvgl.a -lz -lm

run: all
	mkdir -p out
	for s in $(SCENES); do $(OUT)/preview_$$s || exit 1; done
	for l in $(LAYOUTS); do $(OUT)/layout_$$l || exit 1; done

clean:
	rm -rf $(OUT) out

.PHONY: all run clean
