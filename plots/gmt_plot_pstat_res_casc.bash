#!/usr/bin/env bash

#********************************************************************************
# Project:      The Hydraulic Economic River System Simulator (HERSS)
# Filename:     gmt_plot_reservoir.bash
# Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)
# Organization: Å Energi, www.ae.no
#
# This software is released under the MIT license:
# Copyright (c) <2024> <Å Energi, Bernt Viggo Matheussen>
#********************************************************************************

# This script uses Generic Mapping Tools 
# https://www.generic-mapping-tools.org/ 
# gmt --version 6.5.0

# Plot various data for a powerstation and its connected reservoir . 

# It creates a plot with four subplots: 


set -euo pipefail

OUTBASE="pstat_res_casc"

INDIR="../data/res_casc_A/output/"

# node0_RES_A.txt
# node1_RES_B.txt
# node2_CHAN2.txt
# node3_CHAN3.txt
# node4_PSTAT_B.txt
# node5_CHANNEL5.txt

# outstate.txt
# riversystem_ResCascA_output.txt
# reservoirs_ResCascA_out.txt

INPUT="${INDIR}/node1_RES_B.txt"

PSTATINPUT="${INDIR}/node4_PSTAT_B.txt"



gmt begin "${OUTBASE}" png,pdf
    gmt set \
        MAP_FRAME_TYPE plain \
        FONT_TITLE 14p,Helvetica-Bold \
        FONT_LABEL 12p,Helvetica \
        FONT_ANNOT_PRIMARY 10p,Helvetica \
        FONT_ANNOT_SECONDARY 11p,Helvetica \
        FORMAT_TIME_SECONDARY_MAP abbreviated \
        FORMAT_DATE_MAP "o yyyy" \
        TIME_EPOCH 1970-01-01T00:00:00 \
        TIME_UNIT s

    # RESERVOIR node 1 RES_B
    # reservoir_init_fr= 0.46000
    # yyyy mm dd hh [m3/s] [Euro/MWh] [fr] [m3/s] [Mm3] [masl] [fr] [Euro]         [m3/s]     [m3/s]    [m3/s]   [m3/s]    [m3/s] 
    # yyyy mm dd hh Inflow Price Action Up_Inflow Res_Mm3 Res_masl Res_fr lrw_cost tunnelflow hatchflow overflow auto_qmin tot_outflow
    # 2022 9 1 0 1.5000 24.8800 0.7000 0.0000 4.9067 851.7259 0.4341 0.0000 0.0000 4.2000 0.0000 0.0000 4.2000 
    # 2022 9 2 0 1.3500 25.8500 0.8000 0.0000 4.6086 851.4172 0.4010 0.0000 0.0000 4.8000 0.0000 0.0000 4.8000 


    # Plot inflow 
    awk 'NR > 4 {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, $5
    }' "${INPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-0.5/20 \
        -JX16c/4c \
        -f0T \
        -W1.0p,blue \
        -BWSen+t"Testdata Res_Casc_A" \
        -Bpxf1d \
        -Bpya5f1+l"Inflow [m@+3@+/s]" \

    # After the inflow plot:
    echo "(a) RES_B, Node 1 " | gmt text -F+cTL+f11p,Helvetica-Bold -D0.15c/-0.2c

    # Reservoir filling in percent. 
    awk 'NR > 4 {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, 100*$11
    }' "${INPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-2.0/150 \
        -JX16c/4c -Y-4.6c \
        -f0T \
        -W1.0p,blue \
        -BWSen \
        -Bpxf1d \
        -Bpya20f20+l"Reservoir filling [%]"


    # Plot black dotted line at 100% filling level.
    awk 'NR > 4 {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, 100
    }' "${INPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-2.0/150 \
        -JX16c/4c \
        -f0T \
        -W1.0p,black,. \
        -BWSen

    # After the reservoir filling plot:
    echo "(b) RES_B, Node 1 " | gmt text -F+cTL+f11p,Helvetica-Bold -D0.15c/-0.2c


    

    # POWERSTATION node 4 PSTAT_B
    # init_Power = 0.00000
    # penstock_config = SHARED
    # headloss_coef = 0.300000
    # yyyy mm dd hh [m3/s]    [Euro/MWh]  [fr_g0] [fr_g1] [m3/s] [m3/s] [Euro] [Euro] [m] [m] [MWh] [Euro] [Euro] [GWh/Mm3]
    # yyyy mm dd hh Up_Inflow Price Action_g0 Action_g1 tot_outflow auto_qmin income startstopCost Hnetto Hbrutto Power adjust_cost profit est_eekv
    # 2022 9 1 0 6.2000 24.8800 0.8000 0.7500 6.2000 0.0000 1731.5844 2.0000 53.4214 64.9534 69.5974 0.0000 1729.5844 0.1299
    # 2022 9 2 0 3.2000 25.8500 0.8000 0.0000 3.2000 0.0000 1075.3043 1.0000 61.8635 64.9355 41.5978 0.0000 1074.3043 0.1505

    # Actions from both generators
    awk 'NR > 6 {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, $7
    }' "${PSTATINPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-0.1/1.3 \
        -JX16c/4c -Y-4.6c \
        -f0T \
        -W1.0p,black \
        -BWSen \
        -Bpxf1d \
        -Bpya.20f.20+l"Actions [fr]"
        


    awk 'NR > 6 {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, $8
    }' "${PSTATINPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-0.1/1.3 \
        -JX16c/4c \
        -f0T \
        -W1.0p,green \
        -BWSen

    echo "(c) PSTAT_B, Node 4" | gmt text -F+cTL+f11p,Helvetica-Bold -D0.15c/-0.2c


    # Legend for middle panel
    gmt legend -DjTR+w3.2c+o0.1c/0.1c -F+gwhite << EOF
S 0.0c - 0.5c - 1.0p,black 0.4c Generator 0
S 0.0c - 0.5c - 1.0p,green   0.4c Generator 1
EOF


    # Power [MWh], energy from both generators 
    awk 'NR > 6 {
        yyyy = $1; mm = $2; dd = $3; hh = 12
        printf "%04d-%02d-%02dT%02d:00 %s\n", yyyy, mm, dd, hh, $15
    }' "${PSTATINPUT}" | \
    gmt plot \
        -R2022-09-01T00:00/2022-10-01T00:00/-1/110 \
        -JX16c/4c -Y-4.6c \
        -f0T \
        -W1.0p,red \
        -BWSen \
        -Bpxa1d \
        -Bpya20f20+l"Power [MWh]" \
        -Bsxa1O


    echo "(d) PSTAT_B, Node 4" | gmt text -F+cTL+f11p,Helvetica-Bold -D0.15c/-0.2c


gmt end show

