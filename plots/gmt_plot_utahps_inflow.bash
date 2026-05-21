#!/usr/bin/env bash

#********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     gmt_plot_utahps_inflow.bash
# Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
# Organization: Å Energi, www.ae.no#
#
# This software is released under the MIT license:
# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
#********************************************************************************

# This script plots the input time series for the uTAHPS case using GMT.
# https://www.generic-mapping-tools.org/ 
# gmt --version 6.5.0

# It reads the inflow series, price series, and action series from text
# files and creates a plot with three subplots: 
# one for each time series. The x-axis is shared across all subplots
# and represents time, while the y-axes represent the respective values
#  of inflow, price, and action.


set -euo pipefail

OUTBASE="utahps_inflow"

INPUT="../data/utahps_daily/inflowseries_utahps.txt"

#Date_NodeID	0	3	5	9
#20220901	0.35	0.22	0.29	0.64
#20220902	0.31	0.20	0.26	0.58

HEADER="DATE HJELLE GRESSE TOPPSY KROKNESVATN"

STR_STARTDATE="20220901"
STR_ENDDATE="20230831"

gmt begin "${OUTBASE}" png,pdf
    gmt set \
        MAP_FRAME_TYPE plain \
        FONT_TITLE 14p,Helvetica-Bold \
        FONT_LABEL 12p,Helvetica \
        FONT_ANNOT_PRIMARY 10p,Helvetica \
        FONT_ANNOT_SECONDARY 10p,Helvetica \
        FORMAT_TIME_SECONDARY_MAP abbreviated \
        FORMAT_DATE_MAP "o yyyy" \
        TIME_EPOCH 1970-01-01T00:00:00 \
        TIME_UNIT s


    R="-R2022-09-01T00:00/2023-08-31T23:00/-1.0/30.0"
    J="-JX22c/7c"

    # HJELLE (col 2) - first plot sets frame
    awk 'NR > 1 {
        yyyy = substr($1,1,4); mm = substr($1,5,2); dd = substr($1,7,2)
        printf "%s-%s-%sT12:00 %s\n", yyyy, mm, dd, $2
    }' "${INPUT}" | \
    gmt plot $R $J \
        -f0T \
        -W1.5p,blue \
        -BWSen+t"Daily inflow - uTAHPS" \
        -Bpxf1d \
        -Bsxa1O \
        -Bpya5f5+l"Inflow [m@+3@+/s]"

    # GRESSE (col 3)
    awk 'NR > 1 {
        yyyy = substr($1,1,4); mm = substr($1,5,2); dd = substr($1,7,2)
        printf "%s-%s-%sT12:00 %s\n", yyyy, mm, dd, $3
    }' "${INPUT}" | \
    gmt plot $R $J -f0T -W1.5p,red

    # TOPSY (col 4)
    awk 'NR > 1 {
        yyyy = substr($1,1,4); mm = substr($1,5,2); dd = substr($1,7,2)
        printf "%s-%s-%sT12:00 %s\n", yyyy, mm, dd, $4
    }' "${INPUT}" | \
    gmt plot $R $J -f0T -W1.5p,darkgreen

    # KROKNESVATN (col 5)
    awk 'NR > 1 {
        yyyy = substr($1,1,4); mm = substr($1,5,2); dd = substr($1,7,2)
        printf "%s-%s-%sT12:00 %s\n", yyyy, mm, dd, $5
    }' "${INPUT}" | \
    gmt plot $R $J -f0T -W1.5p,orange



    gmt legend -DjTR+w5.0c+o0.1c/0.1c -F+gwhite << EOF
S 0.0c - 0.5c - 1.5p,blue      0.4c HJELLE
S 0.0c - 0.5c - 1.5p,red       0.4c GRESSE
S 0.0c - 0.5c - 1.5p,darkgreen 0.4c TOPSY
S 0.0c - 0.5c - 1.5p,orange    0.4c KROKNESVATN
EOF

gmt end show
