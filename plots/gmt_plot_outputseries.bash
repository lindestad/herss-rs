#!/usr/bin/env bash

#********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     gmt_plot_outputseries.bash
# Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
# Organization: Å Energi, www.ae.no#
#
# This software is released under the MIT license:
# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
#********************************************************************************

# This script plots the output time series for the uTAHPS case using GMT.
# https://www.generic-mapping-tools.org/ 
# gmt --version 6.5.0


set -euo pipefail

OUTBASE="outputseries"

INDIR="../data/mini_utahps_hourly/output/"

NODE0="${INDIR}/node0_HJELLE.txt"
NODE1="${INDIR}/node1_SVOLETJONN.txt"
NODE2="${INDIR}/node2_VANAROSEN.txt"


gmt begin "${OUTBASE}" png,pdf
    gmt set \
        MAP_FRAME_TYPE plain \
        FONT_TITLE 14p,Helvetica-Bold \
        FONT_LABEL 12p,Helvetica \
        FONT_ANNOT_PRIMARY 10p,Helvetica \
        FONT_ANNOT_SECONDARY 11p,Helvetica \
        FORMAT_DATE_MAP yyyy-mm-dd \
        FORMAT_CLOCK_MAP hh:mm \
        TIME_EPOCH 1970-01-01T00:00:00 \
        TIME_UNIT s

    awk 'NR > 4 {
        yyyy = $1; 
        mm   = $2;
        dd   = $3;
        hh   = $4;
        printf "%s-%s-%sT%s:00 %s\n", yyyy, mm, dd, hh, $9
    }' "${NODE0}" | \
    gmt plot \
        -R2026-05-07T00:00/2026-05-08T23:00/6.8/7.4 \
        -JX16c/4c \
        -f0T \
        -W1.0p,blue \
        -BWSen+t"Output timeseries for uTAHPS" \
        -Bpxf1h \
        -Bpya.1f.1+l"Reservoir filling [Mm@+3@+]"

    printf "2026-05-07T00:00 7.35 Hjelle\n" | \
    gmt text \
        -R2026-05-07T00:00/2026-05-08T23:00/6.8/7.4 \
        -JX16c/4c \
        -f0T \
        -F+f11p,Helvetica-Bold+jTL \
        -Dj0.15c/-0.10c


    awk 'NR > 6 {
        yyyy = $1;
        mm   = $2;
        dd   = $3;
        hh   = $4;
        printf "%s-%s-%sT%s:00 %s\n", yyyy, mm, dd, hh, $14
    }' "${NODE1}" | \
    gmt plot \
        -R2026-05-07T00:00/2026-05-08T23:00/-0.1/2.1 \
        -JX16c/4c -Y-4.4c \
        -f0T \
        -W1.0p,red \
        -BWSen \
        -Bpxf1h \
        -Bpya.50f0.5+l"Power [MWh]"

    printf "2026-05-07T00:00 7.35 Svoletjonn\n" | \
    gmt text \
        -R2026-05-07T00:00/2026-05-08T23:00/6.8/7.4 \
        -JX16c/4c \
        -f0T \
        -F+f11p,Helvetica-Bold+jTL \
        -Dj0.15c/-0.10c

    gmt set FORMAT_FLOAT_MAP %.1f
    awk 'NR > 5 {
        yyyy = $1;
        mm   = $2;
        dd   = $3;
        hh   = $4;
        printf "%s-%s-%sT%s:00 %s\n", yyyy, mm, dd, hh, $5
    }' "${NODE2}" | \
    gmt plot \
        -R2026-05-07T00:00/2026-05-08T23:00/-0.1/5.0 \
        -JX16c/4c -Y-4.4c \
        -f0T \
        -W1.0p,black \
        -BWSen \
        -Bpxa6Hf1h \
        -Bsxa1D

    awk 'NR > 5 {
        yyyy = $1;
        mm   = $2;
        dd   = $3;
        hh   = $4;
        printf "%s-%s-%sT%s:00 %s\n", yyyy, mm, dd, hh, $7
    }' "${NODE2}" | \
    gmt plot \
        -R2026-05-07T00:00/2026-05-08T23:00/-0.1/5.0 \
        -JX16c/4c \
        -f0T \
        -W1.0p,green \
        -BWSen \
        -Bpxa6Hf1h+l"Time" \
        -Bsxa1D+l"Date" \
        -Bpya1.0f1.0+l"In/Out [m@+3@+/s]"

    printf "2026-05-07T00:00 7.35 Vanarosen\n" | \
    gmt text \
        -R2026-05-07T00:00/2026-05-08T23:00/6.8/7.4 \
        -JX16c/4c \
        -f0T \
        -F+f11p,Helvetica-Bold+jTL \
        -Dj0.15c/-0.10c





    gmt legend -DjTR -F+p0.5p+gwhite <<EOF
S 0.3c - 0.6c black 1.0p 0.7c Inflow
S 0.3c - 0.6c green 1.0p 0.7c Outflow
EOF


gmt end show
