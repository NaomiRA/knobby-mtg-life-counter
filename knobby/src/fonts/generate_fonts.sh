#!/bin/bash
# Font generation script for Knobby MTG Life Counter
# Requires lv_font_conv on PATH (install via: npm install -g lv_font_conv)
#
# Usage:  ./generate_fonts.sh
# Re-run after changing sizes, weights, or character ranges below.

set -euo pipefail
cd "$(dirname "$0")"

CONV=lv_font_conv
BPP=4
FORMAT=lvgl
LV_INCLUDE="lvgl.h"

# Toggle compression: "true" = smaller flash, more CPU per glyph render
# "false" = larger flash, zero decompression overhead
COMPRESS=false

# Font source files
MONTSERRAT_BOLD=Montserrat-Bold.ttf
MONTSERRAT_REGULAR=Montserrat-Regular.ttf
BELEREN_SMALLCAPS_BOLD=belerensmallcaps-bold.ttf
BELEREN_BOLD=beleren-bold_P1.01.ttf
MPLANTIN=mplantin.ttf

# ---------- Character ranges ----------
# Digits + signs for life total / dice (space, +, -, 0-9, =)
RANGE_DIGITS="0x20,0x2B,0x2D,0x30-0x39,0x3D"

# Full printable ASCII (for general UI text)
RANGE_ASCII="0x20-0x7F"
RANGE_UI_TEXT="0x20-0x7F,0xB0"

# ---------- Compression flag ----------
COMPRESS_FLAG=""
if [ "$COMPRESS" = "true" ]; then
    COMPRESS_FLAG="--no-compress false"
else
    COMPRESS_FLAG="--no-compress"
fi

# ---------- Font definitions ----------
# Each entry: output_name font_file weight size range
generate_font() {
    local name="$1"
    local font="$2"
    local size="$3"
    local range="$4"
    local outfile="${name}.c"

    echo "Generating ${outfile} (size=${size}, bpp=${BPP})..."
    $CONV \
        --font "$font" \
        --bpp "$BPP" \
        --size "$size" \
        --range "$range" \
        --format "$FORMAT" \
        $COMPRESS_FLAG \
        --lv-include "$LV_INCLUDE" \
        -o "$outfile"
}

# ---------- Generate fonts ----------

# Large bold font for life total and dice result
generate_font "lv_font_montserrat_bold_116" "$MONTSERRAT_BOLD" 116 "$RANGE_DIGITS"

# Regular font for life preview total ("= xxx")
generate_font "lv_font_montserrat_regular_48" "$MONTSERRAT_REGULAR" 48 "$RANGE_DIGITS"

# Bold fonts for multiplayer life totals (56 for absolute/tabletop, 44 for centric)
generate_font "lv_font_montserrat_bold_56" "$MONTSERRAT_BOLD" 56 "$RANGE_DIGITS"
generate_font "lv_font_montserrat_bold_44" "$MONTSERRAT_BOLD" 44 "$RANGE_DIGITS"

# Beleren variants for the same numeric display sizes
generate_font "lv_font_belerensmallcaps_bold_116" "$BELEREN_SMALLCAPS_BOLD" 116 "$RANGE_DIGITS"
generate_font "lv_font_beleren_bold_48" "$BELEREN_BOLD" 48 "$RANGE_DIGITS"
generate_font "lv_font_belerensmallcaps_bold_56" "$BELEREN_SMALLCAPS_BOLD" 56 "$RANGE_DIGITS"
generate_font "lv_font_belerensmallcaps_bold_44" "$BELEREN_SMALLCAPS_BOLD" 44 "$RANGE_DIGITS"

# Beleren fonts for general UI text
generate_font "lv_font_beleren_bold_18" "$BELEREN_BOLD" 18 "$RANGE_UI_TEXT"
generate_font "lv_font_beleren_bold_20" "$BELEREN_BOLD" 20 "$RANGE_UI_TEXT"
generate_font "lv_font_beleren_bold_22" "$BELEREN_BOLD" 22 "$RANGE_UI_TEXT"
generate_font "lv_font_beleren_bold_26" "$BELEREN_BOLD" 26 "$RANGE_UI_TEXT"
generate_font "lv_font_beleren_bold_36" "$BELEREN_BOLD" 36 "$RANGE_UI_TEXT"
generate_font "lv_font_mplantin_20" "$MPLANTIN" 20 "$RANGE_UI_TEXT"

echo ""
echo "Done! Generated fonts:"
ls -lh *.c 2>/dev/null || echo "(no .c files found)"
