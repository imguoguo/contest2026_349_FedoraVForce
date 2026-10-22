#!/usr/bin/env bash
############################################################################
# app/ui/tools/gen_fonts.sh
#
# Regenerate the Oswald / Outfit C fonts in app/ui/fonts.
#
# Needs python3 with fontTools and lv_font_conv 1.5.3.  The fonts come from
# https://github.com/google/fonts (ofl/oswald, ofl/outfit), both are
# variable fonts, a static instance is cut for each weight first.
#
#   gen_fonts.sh <Oswald[wght].ttf> <Outfit[wght].ttf>
#
# LV_FONT_CONV overrides the converter, default "npx lv_font_conv@1.5.3".
############################################################################

set -e

OSWALD=$1
OUTFIT=$2
OUT=$(cd "$(dirname "$0")/../fonts" && pwd)
CONV=${LV_FONT_CONV:-npx lv_font_conv@1.5.3}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/gen_fonts.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

if [ ! -f "$OSWALD" ] || [ ! -f "$OUTFIT" ]; then
  echo "usage: $0 <Oswald[wght].ttf> <Outfit[wght].ttf>" >&2
  exit 1
fi

instance()
{
  python3 -m fontTools.varLib.instancer -q -o "$TMP/$2.ttf" "$1" "wght=$3"
}

# name font size characters

convert()
{
  $CONV --no-compress --no-prefilter --bpp 4 --format lvgl --lv-include lvgl/lvgl.h \
        --font "$TMP/$2.ttf" --size "$3" --symbols "$4" \
        --lv-font-name "$1" -o "$OUT/$1.c"
}

UPPER="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
LOWER="abcdefghijklmnopqrstuvwxyz"
DIGITS="0123456789"
ASCII=$(python3 -c 'print("".join(map(chr, range(0x20, 0x7f))))')

# Weights as measured on docs/launcher-v2/images

instance "$OSWALD" oswald-medium 500
instance "$OSWALD" oswald-semibold 600
instance "$OUTFIT" outfit-regular 400
instance "$OUTFIT" outfit-semibold 600

convert ui_font_oswald_300 oswald-semibold 300 "$DIGITS:"
convert ui_font_oswald_120 oswald-medium 120 " $DIGITS$UPPER/%:.-+"
convert ui_font_oswald_64  oswald-medium 64  " $DIGITS$UPPER$LOWER/%:.-+"
convert ui_font_outfit_26  outfit-regular 26 "$ASCII"
convert ui_font_outfit_34  outfit-regular 34 "$ASCII"
convert ui_font_outfit_52  outfit-semibold 52 "$ASCII"
