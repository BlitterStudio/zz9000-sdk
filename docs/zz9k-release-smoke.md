# ZZ9000 SDK v2 Release Smoke Checklist

This checklist validates a packaged SDK v2 build against the matching
SDK-service firmware. Pre-service firmware is not a supported runtime for this
SDK line.

Run the checks from the installed AmigaOS package. Replace sample paths with
local files that are known to load on the test system.

## Service Contract

```text
zz9k-services --check-release
zz9k-services --all
zz9k-info
```

Expected pass signal:

- `zz9k-services --check-release` exits with status `0`.
- The final line is `release check ok`.
- `zz9k-info` prints the SDK ABI, capability names, service list, and
  diagnostics without reporting query failures.

Failure routing:

- Missing service discovery, required service flags, opcode ranges, or version
  major mismatches are firmware/SDK release-pair blockers.
- Mailbox open or query failures belong in the firmware/driver transport path
  before testing higher-level services.

## Core, Memory, And Surface

```text
zz9k-smoke
zz9k-surface-info
zz9k-surfaceops --hold-ticks 0 --loops 1000 --stats --stats-interval 100
```

Expected pass signal:

- All commands complete without timeout or SDK status errors.
- Surface operations report nonzero loop progress and no failed operations.
- RTG display state remains usable after the run.

Failure routing:

- Shared-buffer or surface allocation failures route to the memory/surface
  service.
- Slow but completing `zz9k-surfaceops` while audio is active is contention
  evidence, not by itself a correctness failure.

## Crypto

```text
zz9k-hash --alg sha256
zz9k-hash --alg sha1
zz9k-hash --alg poly1305
zz9k-chacha
zz9k-aead
```

Expected pass signal:

- Hash tools match their built-in vectors.
- ChaCha20 and AEAD tools complete their encrypt/decrypt vector checks.

Failure routing:

- Digest or vector mismatches route to the crypto service before any archive,
  TLS, or browser integration work.

## Compression And Archives

```text
zz9k-inflate --selftest
zz9k-archive l Work:Archives/test.zip
zz9k-archive t Work:Archives/test.zip
zz9k-archive x --dry-run -o RAM:zz9k-smoke Work:Archives/test.zip
zz9k-archive l Work:Archives/test.lha
zz9k-archive t Work:Archives/test.tar.gz
```

Expected pass signal:

- `zz9k-inflate --selftest` completes all built-in compressed payload checks.
- Archive list/test/dry-run commands complete without path-safety, CRC, or
  unsupported-method surprises for the chosen fixtures.

Failure routing:

- `unsupported` diagnostics for formats not advertised by the codec service are
  acceptable only when the fixture intentionally covers unsupported methods.
- CRC, path-safety, overwrite-policy, or streamed extraction failures route to
  `zz9k-archive` before adding more archive formats.

## Image, Viewer, And DataTypes

If the package was installed with descriptors still inactive, activate the
validated DataType descriptors before the MultiView/browser checks:

```text
copy Storage/DataTypes/ZZ9000-JPEG#? TO DEVS:DataTypes/
copy Storage/DataTypes/ZZ9000-PNG#? TO DEVS:DataTypes/
AddDataTypes DEVS:DataTypes/ZZ9000-JPEG
AddDataTypes DEVS:DataTypes/ZZ9000-PNG
AddDataTypes LIST
```

```text
zz9k-jpeg Work:Pictures/test.jpg
zz9k-jpeg --fb --hold 200 Work:Pictures/test.jpg
zz9k-png Work:Pictures/test.png
zz9k-png --fb --hold 200 Work:Pictures/test.png
zz9k-view Work:Pictures/test.jpg
zz9k-view Work:Pictures/test.png
zz9k-dtprobe --client Work:Pictures/test.jpg
zz9k-dtprobe --client Work:Pictures/test.png
MultiView Work:Pictures/test.jpg
MultiView Work:Pictures/test.png
```

Expected pass signal:

- JPEG and PNG decode paths complete without SDK status errors.
- Framebuffer and viewer windows show the image, survive close/resize where the
  command opens a resizable viewer, and restore visible RTG contents.
- DataType descriptors are activated from `Storage/DataTypes`, and
  `AddDataTypes LIST` shows `ZZ9000-JPEG` and `ZZ9000-PNG`.
- DataTypes clients display JPEG and PNG through `zz9k-picture.datatype`.

Failure routing:

- Direct tool decode failures route to the image service or shared-buffer path.
- Viewer restore/occlusion failures route to the image-window clipping path.
- MultiView/browser-only failures route to `zz9k-picture.datatype`.
- Transparent PNGs are a known deferred alpha-behavior area unless they crash.

## Audio And MPEGA

```text
zz9k-mp3 --stats Work:Audio/test.mp3
zz9k-mpega-smoke --trace --null-api-check
zz9k-mpega-smoke --trace --stream-info --stats --freq-max 0 --frames 100 Work:Audio/test.mp3
```

Expected pass signal:

- `zz9k-mp3 --stats` reports nonzero decoded frames/samples and no stream
  status errors.
- `zz9k-mpega-smoke --trace --null-api-check` reports `null-api check ok`.
- The stream-info check reports the installed `mpega.library` version/revision,
  nonzero frames/samples, and completes without decode errors.

Failure routing:

- Direct MP3 failures route to the audio service.
- MPEGA-only failures route to the resident compatibility shim.
- Surface slowdown during full-speed audio diagnostics is measured as
  contention unless the graphics or audio command times out or returns errors.
