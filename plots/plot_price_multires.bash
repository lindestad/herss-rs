#!/usr/bin/env bash

#********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     plot_test.bash
# Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
# Organization: Å Energi, www.ae.no#
#
# This software is released under the MIT license:
# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
#********************************************************************************
set -euo pipefail


# ---------------------------------------------------------------------------
# make_steps FILE COL [N_SKIP]
#   Converts column COL of FILE into a staircase (step-function) time series.
#   Each row produces two output points: (t_i, v_i) and (t_{i+1}, v_i),
#   so the horizontal extent of every step equals the actual time interval.
#   N_SKIP = number of header lines to skip (default 1).
# ---------------------------------------------------------------------------
make_steps() {
    local file=$1
    local col=$2
    local n_skip=${3:-1}
    awk -v col="${col}" -v n_skip="${n_skip}" '
    NR > n_skip {
        cur_iso = substr($1,1,4)"-"substr($1,5,2)"-"substr($1,7,2)"T"substr($1,9,2)":00"
        cur_val = $col
        if (prev_iso != "") {
            print prev_iso, prev_val
            print cur_iso,  prev_val
        }
        prev_iso = cur_iso
        prev_val = cur_val
    }
    END { if (prev_iso != "") print prev_iso, prev_val }
    ' "${file}"
}
#--------------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# draw_zone X1 X2 YMAX COLOR
#   Fills a rectangular zone from X1 to X2 (time) and 0 to YMAX.
# ---------------------------------------------------------------------------
draw_zone() {
    printf "%s 0\n%s 0\n%s %s\n%s %s\n" "$1" "$2" "$2" "$3" "$1" "$3"
}


OUTBASE="ptest"

# Date range: one year of hourly data
REGION="2022-09-01T00:00/2023-08-31T23:00/0/800"

OUTBASE="utahps_multires"

INPUT_INFLOW="../data/utahps_multires/inflow.txt"

INPUT_PRICE="../data/utahps_multires/price.txt"

# Zone boundary timestamps
XMIN="2022-09-01T00:00"
Z1="2022-11-01T00:00"   # end of hourly zone / start of daily zone
Z2="2023-01-01T00:00"   # end of daily zone  / start of weekly zone
XMAX="2023-09-01T00:00"

PRICEMAX=800

make_steps "${INPUT_PRICE}" 2 2 > price_steps.txt

gmt begin "${OUTBASE}" png,pdf

    # Insert basemap here. 
    gmt set \
        MAP_FRAME_TYPE plain \
        FONT_TITLE    14p,Helvetica-Bold \
        FONT_LABEL    12p,Helvetica \
        FONT_ANNOT_PRIMARY   10p,Helvetica \
        FONT_ANNOT_SECONDARY 10p,Helvetica \
        FORMAT_TIME_SECONDARY_MAP abbreviated \
        FORMAT_DATE_MAP "o yyyy" \
        TIME_EPOCH  1970-01-01T00:00:00 \
        TIME_UNIT   s

    gmt basemap -R$REGION -JX24c/8c -f0T -BwSen -Bpxf1d -Bsxa1O -Bpy

    draw_zone "${XMIN}" "${Z1}" "${PRICEMAX}" | \
    gmt plot -R$REGION -JX24c/8c -f0T -L -Gcornflowerblue@65 -W0p

    draw_zone "${Z1}" "${Z2}" "${PRICEMAX}" | \
    gmt plot -R$REGION -JX24c/8c -f0T -L -Gpalegreen@65 -W0p

    draw_zone "${Z2}" "${XMAX}" "${PRICEMAX}" | \
    gmt plot -R$REGION -JX24c/8c -f0T -L -Gmoccasin -W0p


    # Override the map settings and plot new map 
    gmt set \
        MAP_FRAME_TYPE plain \
        FONT_TITLE    14p,Helvetica-Bold \
        FONT_LABEL    12p,Helvetica \
        FONT_ANNOT_PRIMARY  10p,Helvetica \
        FORMAT_DATE_MAP mm \
        FORMAT_CLOCK_MAP    hh:mm \
        TIME_EPOCH  1970-01-01T00:00:00 \
        TIME_UNIT   s
    
    awk ' { print $0; } ' price_steps.txt | \
    gmt plot \
        -R"${REGION}" -JX24c/8c -f0T -W0.5p,red \
        -BWSen+t"Multi-resolution Electricity Price" \
        -Bpxf1o -Bsxf1O -Bpya100f50+l"Price [Euro/MWh]"


    echo "2022-10-01T00:00 745 Hourly (1 h)"      | gmt text -R$REGION -JX24c/8c -f0T -F+f9p,Helvetica-Bold,gray25+jCT
    echo "2022-12-01T00:00 745 Daily (24 h avg)"   | gmt text -R$REGION -JX24c/8c -f0T -F+f9p,Helvetica-Bold,gray25+jCT
    echo "2023-05-01T00:00 745 Weekly (168 h avg)" | gmt text -R$REGION -JX24c/8c -f0T -F+f9p,Helvetica-Bold,gray25+jCT


gmt end show

# Clean tmp files 
rm price_steps.txt