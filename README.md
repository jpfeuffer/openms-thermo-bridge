# OpenMS Thermo Raw File Bridge

A minimal Linux bridge that embeds the .NET 8 runtime inside a native C++ executable and exposes Thermo RAW scan counting through the official ThermoFisher RawFileReader libraries.

## What This Does
- Downloads the official ThermoFisher .NET 8 packages from the upstream RawFileReader repository
- Publishes a managed bridge with an `UnmanagedCallersOnly` `GetScanCount` entry point
- Builds a native Linux host that loads .NET through `nethost` and `hostfxr`
- Prints the number of scans for a Thermo `.raw` file

## Architecture
```text
native caller / CLI
       |
thermo_host
       |
hostfxr + .NET 8 runtime
       |
ThermoWrapperManaged.dll
       |
ThermoFisher.CommonCore.RawFileReader
       |
.raw file
```

## Build
```bash
./build_linux.sh
```

This produces:

```text
artifacts/publish/thermo_host
artifacts/publish/managed/ThermoWrapperManaged.dll
artifacts/publish/managed/ThermoWrapperManaged.runtimeconfig.json
```

## Run
```bash
./artifacts/publish/thermo_host /path/to/file.raw
```

Expected output:

```text
Scan count: <number>
```

For a reproducible validation run, this repository uses the public `ginkgotoxin-ms-positive.raw` sample from `HegemanLab/small-thermo-raw-file-examples`, which currently reports:

```text
Scan count: 90
```

## Requirements
- Linux x64
- .NET SDK with the Linux `nethost` pack installed
- A C++17 compiler
- Network access to `raw.githubusercontent.com` and `api.nuget.org`

## Status
- [x] Linux x64 host build
- [x] .NET 8 embedding
- [x] `GetScanCount` bridge
