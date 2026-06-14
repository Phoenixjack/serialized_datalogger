# serialized_datalogger

A lightweight Raspberry Pi Pico W / RP2040 SD-card data logger for serial-style CSV logging.

This project was built as a small embedded utility for saving incoming data to an SD card with session-based filenames, file rollover, index tracking, and optional epoch-time correlation.

It is intended for bench testing, sensor logging, hardware experiments, and quick data capture where a full logging framework would be overkill.

## Project Status

Prototype / work-in-progress.

The logger is functional enough to document and preserve, but it should still be treated as a development utility rather than a finished production library.

## Target Hardware

This project targets:

* Raspberry Pi Pico W
* RP2040-based Arduino environment
* SPI-connected SD card module

The code currently uses:

```cpp
#include <SPI.h>
#include <RP2040_SD.h>
```

It also uses RP2040/Pico-specific includes for platform and random number support.

## Purpose

The goal is to provide a simple way to:

* initialize an SD card
* start a new logging session
* write CSV-style data
* roll over to new files when a file size limit is reached
* maintain an index file for the session
* optionally record a time reference between `millis()` and an external epoch timestamp
* read files back over Serial
* list SD card contents over Serial

## How It Works

Each logging session receives a 16-bit session ID.

That session ID is used as the prefix for generated files.

A session may include:

| File Type  | Filename Pattern | Purpose                           |
| ---------- | ---------------- | --------------------------------- |
| Index file | `XXXXindx.txt`   | Tracks associated data files      |
| Time file  | `XXXXtime.txt`   | Stores epoch / uptime correlation |
| Data files | `XXXXXXXX.csv`   | Stores logged CSV-style data      |

The logger writes data to sequential CSV files and rolls over to a new data file once the current file reaches the configured size limit.

The current rollover limit is:

```text
100 KB per data file
```

## Serial Demo Sketch

The included `.ino` file provides a simple Serial command interface for testing.

On startup, it:

1. opens Serial at `9600`
2. initializes the SD card
3. reports whether the card is ready

## Serial Commands

Commands are entered over Serial.

| Command       | Description                                                       |
| ------------- | ----------------------------------------------------------------- |
| `Sxxxx`       | Start a new session using `xxxx` as comma-separated column labels |
| `D`           | List all files on the SD card                                     |
| `R`           | Read the current data file                                        |
| `Rfilename`   | Read a specific file                                              |
| `Tyyyy`       | Mark current epoch time in seconds                                |
| anything else | Save the received text as a data row                              |

## Example Serial Use

Start a new session with column labels:

```text
Stemp_c,humidity_pct,voltage_v
```

Log a data row:

```text
24.8,51.2,4.97
```

Record an epoch timestamp:

```text
T1733850000
```

List SD card contents:

```text
D
```

Read the current file:

```text
R
```

Read a specific file:

```text
R12340000.csv
```

## Data Format

For normal data input, the demo sketch prepends the current `millis()` value in hexadecimal before writing the row.

Example concept:

```text
1a2b3c,temp_c,humidity_pct,voltage_v
```

The logged data is intended to be easy to import into a spreadsheet or parse later with a script.

## File Rollover

The logger closes the file after each write to help ensure data is actually saved to the SD card.

This is not the fastest possible approach, but it is practical for low-rate logging where data preservation matters more than maximum write throughput.

The current implementation rolls over to a new data file when the active file exceeds the configured file size limit.

## Time Reference

The logger can record a relationship between:

* external epoch time
* current `millis()` uptime
* calculated boot time

This allows logged `millis()` values to be correlated with real-world time after the fact.

The time file is only created after a valid timestamp is provided.

## Repository Contents

| File                       | Purpose                                                     |
| -------------------------- | ----------------------------------------------------------- |
| `serializeddatalogger.ino` | Serial demo / test interface                                |
| `serializeddatalogger.h`   | SD card session logging helper                              |

## Known Limitations

* Prototype code
* Targeted at Raspberry Pi Pico W / RP2040 Arduino environment
* Depends on `RP2040_SD`
* Filenames are constrained by the underlying SD/FAT handling
* File writes still block briefly while data is written
* Files are closed after every write for data safety, not maximum speed
* No formal performance testing
* No production-readiness claim
* No warranty or support commitment

## Cleanup Notes

Recommended cleanup:

* keep the V2 `.ino` and `.h` files
* move V1 to an `archive/` folder or remove it
* add an MIT `LICENSE` file
* eventually rename files to simpler names if this becomes a reusable library

## Possible Future Improvements

Possible future improvements:

* split implementation into `.h` and `.cpp`
* add a cleaner class name
* add examples folder
* add sample output files
* add SD wiring diagram
* add configurable file rollover size
* add clearer status/error reporting
* add non-blocking buffering
* add CSV escaping or stricter row handling
* add automated timestamp formatting
* add documentation for SD card wiring

## License

This project is released under the MIT License.

You are free to use, modify, and adapt it for your own projects. No warranty is provided, and no ongoing support or maintenance is implied.
