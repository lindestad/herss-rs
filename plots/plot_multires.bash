#!/usr/bin/env bash

#********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     plot_multires.bash
# Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
# Organization: Å Energi, www.ae.no
#
# This software is released under the MIT license:
# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
#********************************************************************************

# Plots multi-resolution input data for the uTAHPS case.
# Visualises three temporal zones as a step-function (staircase) plot:
#   Sep-Oct 2022  : hourly data  (narrow steps)
#   Nov-Dec 2022  : daily data   (medium steps, 24-hour average)
#   Jan-Aug 2023  : weekly data  (wide steps, 168-hour average)
#
# GMT 6.5.0  https://www.generic-mapping-tools.org/


set -euo pipefail

OUTBASE="utahps_multires"

INPUT_INFLOW="../data/utahps_multires/inflow.txt"
INPUT_PRICE="../data/utahps_multires/price.txt"

# Zone boundary timestamps
Z1="2022-11-01T00:00"   # end of hourly zone / start of daily zone
Z2="2023-01-01T00:00"   # end of daily zone  / start of weekly zone
XMIN="2022-09-01T00:00"
XMAX="2023-09-01T00:00"


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

# ---------------------------------------------------------------------------
# draw_zone X1 X2 YMAX COLOR
#   Fills a rectangular zone from X1 to X2 (time) and 0 to YMAX.
# ---------------------------------------------------------------------------
draw_zone() {
    printf "%s 0\n%s 0\n%s %s\n%s %s\n" "$1" "$2" "$2" "$3" "$1" "$3" | \
        gmt plot $R $J -f0T -L -G"$4" -W0p
}




# ---------------------------------------------------------------------------
gmt begin "${OUTBASE}" png,pdf


    gmt set \
        MAP_FRAME_TYPE plain \
        FONT_TITLE 14p,Helvetica-Bold \
        FONT_LABEL 12p,Helvetica \
        FONT_ANNOT_PRIMARY 10p,Helvetica \
        FONT_ANNOT_SECONDARY 10p,Helvetica \
        FORMAT_TIME_SECONDARY_MAP abbreviated \
        FORMAT_DATE_MAP "o"

    gmt subplot begin 2x1 -Fs22c/7c -M0.3c/0.6c -A

        # ====================================================================
        # Panel (a): Inflow  [m³/s]
        # ====================================================================
        gmt subplot set 0
        R="-R${XMIN}/${XMAX}/0/35"
        J="-JX22c/7c"

        #draw_zone "${XMIN}" "${Z1}" 35 cornflowerblue@65
        #draw_zone "${Z1}"   "${Z2}" 35 palegreen@65
        #draw_zone "${Z2}" "${XMAX}" 35 moccasin@65

        gmt basemap $R $J \
            -BWSen+t"Inflow - uTAHPS (multi-resolution)" \
            -Bpxa1Of1d \
            -Bpya5f5+l"Inflow [m@+3@+/s]"

        printf "%s 0\n%s 35\n" "${Z1}" "${Z1}" | gmt plot $R $J -f0T -W1p,gray40,--
        printf "%s 0\n%s 35\n" "${Z2}" "${Z2}" | gmt plot $R $J -f0T -W1p,gray40,--

        make_steps "${INPUT_INFLOW}" 2 | gmt plot $R $J -f0T -W1p,blue
        make_steps "${INPUT_INFLOW}" 3 | gmt plot $R $J -f0T -W1p,red
        make_steps "${INPUT_INFLOW}" 4 | gmt plot $R $J -f0T -W1p,darkgreen
        make_steps "${INPUT_INFLOW}" 5 | gmt plot $R $J -f0T -W1p,orange

        echo "2022-10-01T00:00 32.5 Hourly (1 h)"      | gmt text $R $J -f0T -F+f9p,Helvetica-Bold,gray25+jCT
        echo "2022-12-01T00:00 32.5 Daily (24 h avg)"   | gmt text $R $J -f0T -F+f9p,Helvetica-Bold,gray25+jCT
        echo "2023-05-01T00:00 32.5 Weekly (168 h avg)" | gmt text $R $J -f0T -F+f9p,Helvetica-Bold,gray25+jCT

        gmt legend -DjBR+w5.0c+o0.2c/4.2c -F+gwhite << EOF
S 0.0c - 0.5c - 1.5p,blue       0.4c HJELLE
S 0.0c - 0.5c - 1.5p,red        0.4c GRESSE
S 0.0c - 0.5c - 1.5p,darkgreen  0.4c TOPSY
S 0.0c - 0.5c - 1.5p,orange     0.4c KROKNESVATN
EOF

        # ====================================================================
        # Panel (b): Electricity price  [NOK/MWh]
        # ====================================================================
        gmt subplot set 1
        R="-R${XMIN}/${XMAX}/0/700"
        J="-JX22c/7c"

        draw_zone "${XMIN}" "${Z1}" 700 cornflowerblue@65
        draw_zone "${Z1}"   "${Z2}" 700 palegreen@65
        draw_zone "${Z2}" "${XMAX}" 700 moccasin@65

        gmt basemap $R $J \
            -BWSen \
            -Bpxa1Of1d \
            -Bpya100f100+l"Price [NOK/MWh]"

        printf "%s 0\n%s 700\n" "${Z1}" "${Z1}" | gmt plot $R $J -f0T -W1p,gray40,--
        printf "%s 0\n%s 700\n" "${Z2}" "${Z2}" | gmt plot $R $J -f0T -W1p,gray40,--

        # price file has 2 header lines → n_skip=2
        make_steps "${INPUT_PRICE}" 2 2 | gmt plot $R $J -f0T -W1p,darkviolet

        echo "2022-10-01T00:00 645 Hourly (1 h)"      | gmt text $R $J -f0T -F+f9p,Helvetica-Bold,gray25+jCT
        echo "2022-12-01T00:00 645 Daily (24 h avg)"   | gmt text $R $J -f0T -F+f9p,Helvetica-Bold,gray25+jCT
        echo "2023-05-01T00:00 645 Weekly (168 h avg)" | gmt text $R $J -f0T -F+f9p,Helvetica-Bold,gray25+jCT

    gmt subplot end

gmt end show
