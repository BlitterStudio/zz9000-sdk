# AGENTS.md

## Project Overview

ZZ9000 SDK v2 (BlitterStudio fork) — C99 cross-compilation SDK for AmigaOS targeting m68k architecture. The ZZ9000 is a graphics/ARM coprocessor card; this repo provides the Amiga-side library (`zz9k.library`), CLI tools, DataTypes, and developer headers that talk to firmware services on the ARM side via a mailbox ABI.

**License:** GPL-3.0-or-later. Copyright (C) 2024-2026 Dimitris Panokostas / BlitterStudio.

## Build Commands

All AmigaOS builds run inside Docker. Two script variants exist (PowerShell and POSIX shell) — pick whichever matches your host:

```powershell
# PowerShell (Windows)
powershell -ExecutionPolicy Bypass -File scripts\build-m68k-amigaos.ps1
powershell -ExecutionPolicy Bypass -File scripts\package-m68k-amigaos.ps1
```

```sh
# Shell (Linux/macOS/WSL)
./scripts/build-m68k-amigaos.sh
./scripts/package-m68k-amigaos.sh
```

Docker image: `sacredbanana/amiga-compiler:m68k-amigaos`. Compiler is `m68k-amigaos-gcc` with flags `-noixemul -Os -s`.

Package output: `build/package/amigaos3/` (includes `MANIFEST.sha256`).

### Host-Side Tests (Native CMake)

Tests compile and run natively on your host machine — no cross-compiler needed:

```sh
cmake -B build-cmake && cmake --build build-cmake && ctest --test-dir build-cmake
```

## Directory Layout

| Directory | Purpose |
|---|---|
| `include/zz9k/` | Public SDK headers (application-facing umbrella + helpers) |
| `host/include/zz9k/` | Host helper headers (request/reply builders, mailbox types) |
| `host/src/` | Host implementation (`zz9k_host.c`) — NOT AmigaOS code |
| `amiga/src/` | `zz9k.library` resident + core library implementation |
| `amiga/include/` | AmigaOS dev headers: `proto/`, `clib/`, `inline/`, `pragmas/`, `libraries/`, `zz9k/event_wait.h` |
| `amiga/fd/` | Function descriptor files (`zz9k_lib.fd`, `mpega*.fd`) for stub generation |
| `amiga/mpega/` | MPEGA library resident (compatibility shim) |
| `amiga/datatypes/` | `zz9k-picture.datatype` source + descriptors (`.b64`, `.dtid`, `.info`) |
| `tools/` | CLI tool implementations (`zz9k-info.c`, `zz9k-jpeg.c`, etc.) |
| `tools/lha-unix/` | Third-party LHa decoder subset (jca02266/lha 1.14i) |
| `examples/amiga-*/` | Example programs (library, jpeg-stream, typed-decode, crypto) |
| `amiga/provider/` | ZZ9000 OpenSSL 3 provider (crypto offload for AmiSSL) |
| `integration/amissl/` | AmiSSL drop-in build: pinned ref, patch, build script |
| `tests/` | ~80 C test executables (host-side, CMake-driven) |
| `docs/` | Prose documentation (`zz9k-library.md`, `zz9k-modules.md`, etc.) |
| `scripts/` | Build and package scripts (.ps1 + .sh pairs) |

## Key Conventions & Gotchas

### Two Build Systems, Different Purposes

- **Docker scripts** → cross-compile everything for AmigaOS m68k (libraries, tools, datatypes, examples). Output goes to `build/`.
- **CMakeLists.txt** → native host builds ONLY for tests and static libraries. Never produces Amiga binaries. Uses C99 strict (`-std=c99 -pedantic`).

### Test Categories

Tests fall into two patterns — both run natively via CMake:

1. **Logic/unit tests** (e.g., `caps_helper_test.c`, `smoke_logic_test.c`) — self-contained, no CLI arguments.
2. **Source tests** (e.g., `package_script_test.c`, `m68k_build_script_test.c`) — receive file paths as argv and parse/validate source files. These verify that build scripts, docs, tool sources, and metadata stay in sync.

### Compiler Flags Matter

AmigaOS builds use `-noixemul` (no ixemul emulation layer) and `-nostartfiles` for resident libraries. Tools that link `zz9k_host.o` do NOT include `amiga/include`; tools that call `zz9k.library` LVOs DO include it. The build script distinguishes `$CFLAGS` vs `$LIBCFLAGS`.

### DataType Descriptors Are Base64-Embedded

JPEG/PNG datatype descriptor binaries live as `.b64` files in `amiga/datatypes/descriptors/`. The package scripts decode them during packaging. Do not edit the decoded output — edit the source and re-base64.

### Package Script Path Safety

The PowerShell package script enforces that `$OutputDir` stays under `build/package/`. Changing this path requires modifying the validation logic.

### CI Covers Only the AmiSSL Build

One workflow exists: `.github/workflows/build-amissl-provider.yml` builds the
ZZ9000-provider-enabled `amissl.library` (see `integration/amissl/`). There is
no CI for the SDK itself, no pre-commit hooks, no linting config. Quality gates
are the CMake test suite and manual hardware smoke tests (documented in
`docs/zz9k-release-smoke.md`).

### Provider Tests Need OpenSSL 3

The `provider_*` host tests build only when CMake finds an OpenSSL 3
development install (`find_package(OpenSSL 3.0)`). On hosts without it (e.g. a
plain Windows setup) they are silently skipped — run the suite in a Linux
container with `libssl-dev` to cover the provider.

### Third-Party Code Boundary

`tools/lha-unix/` is third-party (LHa for UNIX). Retains original redistribution terms. Do not modify without checking license compatibility.

## Development Workflow

1. Make changes to source files.
2. Run host tests: `cmake -B build-cmake && cmake --build build-cmake && ctest --test-dir build-cmake`
3. Cross-compile for AmigaOS: run the appropriate build script.
4. Package: run the package script (runs build first unless `--skip-build` / `-SkipBuild`).
5. Hardware validation follows `docs/zz9k-release-smoke.md`.

## Documentation Entry Points

- `docs/zz9k-library.md` — primary reference for AmigaOS-side API usage
- `docs/zz9k-modules.md` — firmware service metadata shape
- `docs/zz9k-picture-datatype.md` — DataType class details and activation
- `docs/zz9k-release-smoke.md` — hardware validation checklist
