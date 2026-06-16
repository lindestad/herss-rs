> [!IMPORTANT]
> This repository is experimental and is not the authoritative HERSS implementation.
> The only correct mainline version is [berntmath/herss](https://github.com/berntmath/herss).
> All credit for HERSS goes to Bernt Viggo Matheussen.

## Rust rewrite

This repository now contains a Rust implementation of the HERSS simulator. The
Cargo crate builds a `herss` binary and exposes the simulator as a library for
tests and future tooling.

```sh
cargo build
cargo test
cargo run -- data/mini_utahps_hourly/global.txt
```

The Rust port keeps the existing HERSS text input and output formats. Regression
tests compare generated output against checked-in golden data for representative
reservoir, channel, powerstation, and new action-header cases.

Project:      The Hydraulic Economic River System Simulator (HERSS)

Filename:     README.md

Startdate:	  April, 2024
Last update:  June, 2026

Developer:    Bernt Viggo Matheussen (Bernt.Viggo.Matheussen@aenergi.no)

Organization: Å Energi, www.ae.no

This software is released under the MIT license, see LICENCE.md for details.

For instructions on how to install and run the HERSS model please read the user manual.
It is located under the ./doc folder. Look for the file herss.pdf

User manual:  herss.pdf
