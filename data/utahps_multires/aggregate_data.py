#!/home/bernt/py_env/herss_env/bin/python
# -*- coding: utf-8 -*-

"""
 * Project:       HERSS
 * Filename:      aggregate_data.py
 * Developers:    Bernt Viggo Matheussen
 * Original date: 21 May, 2026
 *
 * Reads hourly inflow, price and action files and writes new files with
 * mixed temporal resolution:
 *   - First 2 months  : hourly   (Sep-Oct 2022)
 *   - Next  2 months  : daily    (Nov-Dec 2022)  -- mean of 24 hourly values
 *   - Remainder       : weekly   (Jan-Aug 2023)  -- mean of 168 hourly values
 *
 * Output files written to the local directory:
 *   inflow.txt, price.txt, actions.txt
"""

import sys
import os
from datetime import datetime, timedelta

# ---------------------------------------------------------------------------
# Helper: read a generic tab/space separated file.
# Returns (header_lines, list of [datetime, [float, ...]])
# ---------------------------------------------------------------------------
def read_data_file(filename, n_header_lines):
    header_lines = []
    rows = []
    with open(filename, "r") as f:
        for i, line in enumerate(f):
            line = line.rstrip("\n")
            if i < n_header_lines:
                header_lines.append(line)
                continue
            parts = line.split()
            if not parts:
                continue
            dt = datetime.strptime(parts[0], "%Y%m%d%H")
            values = [float(v) for v in parts[1:]]
            rows.append((dt, values))
    return header_lines, rows


# ---------------------------------------------------------------------------
# Aggregate rows into groups of `group_size`.
# Returns list of (first_dt_in_group, [mean_col1, mean_col2, ...])
# ---------------------------------------------------------------------------
def aggregate_groups(rows, group_size):
    result = []
    i = 0
    while i < len(rows):
        group = rows[i : i + group_size]
        first_dt = group[0][0]
        n_cols = len(group[0][1])
        means = []
        for c in range(n_cols):
            means.append(sum(r[1][c] for r in group) / len(group))
        result.append((first_dt, means))
        i += group_size
    return result


# ---------------------------------------------------------------------------
# Write output file.
# header_lines : list of strings (written verbatim)
# rows         : list of (datetime, [float, ...])
# col_fmt      : format string for each value column
# ---------------------------------------------------------------------------
def write_data_file(filename, header_lines, rows, col_fmt="%.4f"):
    with open(filename, "w") as f:
        for h in header_lines:
            f.write(h + "\n")
        for dt, values in rows:
            ts = dt.strftime("%Y%m%d%H")
            val_str = "\t".join(col_fmt % v for v in values)
            f.write(f"{ts}\t{val_str}\n")
    print(f"Written {len(rows)} rows to {filename}")


# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------
if __name__ == "__main__":

    print("aggregate_data.py")

    inflow_filename  = "../utahps_hourly/inflowseries_utahps.txt"
    price_filename   = "../utahps_hourly/pricefile_utahps.txt"
    actions_filename = "../utahps_hourly/actions_utahps.txt"

    # Boundary datetimes (inclusive end of each zone)
    end_hourly = datetime(2022, 10, 31, 23)   # up to and including Oct 31 23:00
    end_daily  = datetime(2022, 12, 31, 23)   # up to and including Dec 31 23:00
    # everything after end_daily is aggregated weekly

    # ------------------------------------------------------------------
    # Read input files
    # Price file has 2 header lines; inflow and actions have 1 each
    # ------------------------------------------------------------------
    inflow_headers,  inflow_rows  = read_data_file(inflow_filename,  1)
    price_headers,   price_rows   = read_data_file(price_filename,   2)
    actions_headers, actions_rows = read_data_file(actions_filename, 1)

    # ------------------------------------------------------------------
    # Split into three zones
    # ------------------------------------------------------------------
    def split_zones(rows):
        hourly = [(dt, v) for dt, v in rows if dt <= end_hourly]
        daily  = [(dt, v) for dt, v in rows if end_hourly < dt <= end_daily]
        weekly = [(dt, v) for dt, v in rows if dt > end_daily]
        return hourly, daily, weekly

    inflow_h,  inflow_d,  inflow_w  = split_zones(inflow_rows)
    price_h,   price_d,   price_w   = split_zones(price_rows)
    actions_h, actions_d, actions_w = split_zones(actions_rows)

    print(f"Hourly zone : {len(inflow_h)} rows")
    print(f"Daily zone  : {len(inflow_d)} rows  -> {len(inflow_d)//24} days")
    print(f"Weekly zone : {len(inflow_w)} rows  -> {len(inflow_w)//168} full weeks + {len(inflow_w)%168} remaining hours")

    # ------------------------------------------------------------------
    # Aggregate daily (24 h) and weekly (168 h)
    # ------------------------------------------------------------------
    inflow_out  = inflow_h  + aggregate_groups(inflow_d,  24)  + aggregate_groups(inflow_w,  168)
    price_out   = price_h   + aggregate_groups(price_d,   24)  + aggregate_groups(price_w,   168)
    actions_out = actions_h + aggregate_groups(actions_d, 24)  + aggregate_groups(actions_w, 168)

    # ------------------------------------------------------------------
    # Write output files to local directory
    # ------------------------------------------------------------------
    script_dir = os.path.dirname(os.path.abspath(__file__))
    write_data_file(os.path.join(script_dir, "inflow.txt"),  inflow_headers,  inflow_out,  "%.4f")
    write_data_file(os.path.join(script_dir, "price.txt"),   price_headers,   price_out,   "%.2f")
    write_data_file(os.path.join(script_dir, "actions.txt"), actions_headers, actions_out, "%.4f")

    print("THE-END")
#---------------------------------------------------
