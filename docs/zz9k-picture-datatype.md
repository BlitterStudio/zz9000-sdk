# ZZ9000 Picture DataType

Copyright (C) 2024-2026, Dimitris Panokostas / BlitterStudio

`zz9k-picture.datatype 42.147` is the validated SDK v2 DataType candidate for
the current package. It is packaged as a side-by-side subclass of the system
`picture.datatype` and must not replace `Classes/DataTypes/picture.datatype`.
OS3.1 remains the minimum target: the class opens against
`picture.datatype` v39 and dynamically uses newer superclass features only
when they are available.

The class binary installs as:

```text
Classes/DataTypes/zz9k-picture.datatype
```

The JPEG and PNG recognition descriptors are packaged inactive under `Storage/DataTypes`
so activation is an explicit install step:

```text
Storage/DataTypes/ZZ9000-JPEG
Storage/DataTypes/ZZ9000-JPEG.info
Storage/DataTypes/ZZ9000-PNG
Storage/DataTypes/ZZ9000-PNG.info
```

To activate the validated DataType path on a test or release-install system,
install the class and copy the descriptors into `DEVS:DataTypes`:

```text
copy Classes/DataTypes/zz9k-picture.datatype TO SYS:Classes/DataTypes/
copy Storage/DataTypes/ZZ9000-JPEG#? TO DEVS:DataTypes/
copy Storage/DataTypes/ZZ9000-PNG#? TO DEVS:DataTypes/
AddDataTypes DEVS:DataTypes/ZZ9000-JPEG
AddDataTypes DEVS:DataTypes/ZZ9000-PNG
AddDataTypes LIST
```

`AddDataTypes LIST` should show `ZZ9000-JPEG` and `ZZ9000-PNG` before
MultiView or browser clients are expected to route matching files to
`zz9k-picture.datatype`. Keeping the descriptors in `Storage/DataTypes` by
default prevents accidental global routing on systems that only want the SDK
tools or manual smoke tests.

The package also includes `C:zz9k-dtprobe` for isolating DataTypes failures:

```text
zz9k-dtprobe XYZ.png
zz9k-dtprobe --client XYZ.png
zz9k-dtprobe --read-pixels XYZ.png
zz9k-dtprobe --screen-remap --layout --client --color-tables XYZ.png
zz9k-dtprobe --draw-window XYZ.png
```

If the descriptor does not match, `NewDTObject` should fail with a DataTypes
lookup error. If the descriptor matches but object creation fails inside the
class, the command reports the `IoErr()` value from that class path.
The `--client` mode also queries bitmap and mask attributes that applications
such as image viewers and browsers may request from picture objects.
The `--read-pixels` mode reads row checksums through `PDTM_READPIXELARRAY`
for comparing the pixel data returned by different picture datatypes.
The `--screen-remap`, `--layout`, and `--color-tables` modes provide a closer
client-style probe by passing the public screen/remap context, running
`DTM_PROCLAYOUT`, and printing palette/remap table attributes.
The `--draw-window` mode opens a temporary public-screen window, runs layout
if needed, adds the datatype object to that window, refreshes it, and hashes
the actual screen pixels over a capped diagnostic rectangle.

Hardware testing confirmed the `42.135` rollback no longer crashes, and the
follow-up benchmark run showed no regressions. The failed `42.133`/`42.134`
experiments show that requesting RGBA8888 firmware tiles and then writing RGB
rows through an otherwise opaque v43 object is not a safe compatibility path.
After checking the TGAdt/JFIFdt44 reference datatype sources, `42.140` adds two
opt-in diagnostic render modes for hardware validation. Use `alphareference`
for the normal layout path and `alphareferencenolayout` for the layout-skip
probe:

```text
echo alphareference > ENV:ZZ9K_PICTURE_RENDER_MODE
echo alphareferencenolayout > ENV:ZZ9K_PICTURE_RENDER_MODE
```

These modes publish a generated RGBA reference picture through the v43
`PDTM_WRITEPIXELARRAY` path with `bmh_Masking = mskHasAlpha`, `PBPAFMT_RGBA`,
`PDTA_Remap` disabled, and `PDTA_AlphaChannel` set. They are intended to verify
the newer picture.datatype alpha contract on OS3.2 without changing normal
JPEG/PNG decode behavior.

The `42.146` path keeps that validated real transparent PNG decode contract
and disables the JPEG v47 RGB direct path. On `picture.datatype v47`, JPEG and
PNG both use the validated v43 `PDTM_WRITEPIXELARRAY` path. PNG stays on
`PDTM_WRITEPIXELARRAY`; alpha PNGs keep their alpha state, request `RGBA8888`
tiles from the SDK image service, prepare the v43 picture object with
`bmh_Masking = mskHasAlpha`, and write `PBPAFMT_RGBA` pixels through the
superclass.

The earlier direct attempts remain documented as regressions: `42.142` showed
black JPEG output and slower BGRA conversion behavior, and `42.143` was worse
with JPEG decode around 7 seconds plus incorrect repeated grayscale-looking
output. `42.145` avoids those paths by refusing indexed, grey, RGBA, ARGB, and
conversion-based direct buffers for JPEG.
That benchmark path produced correct-enough output for timing but averaged
about 2.57 seconds, well slower than the previous v43 write-pixel baseline, so
the JPEG v47 RGB direct path is disabled in `42.146` with
`ZZ9K_PICTURE_ENABLE_JPEG_DATATYPE_V47_RGB_DIRECT` set to 0.

`42.147` is a stability hardening release with no behaviour change on correct
input. The three LUT8 palette functions (`zz9k_picture_prepare_lut8_palette`,
`zz9k_picture_prepare_legacy_lut8_palette`, and the disabled alpha variant)
now use the correct `SetDTAttrs(PDTA_NumColors)` → `GetDTAttrs(PDTA_ColorRegisters,
PDTA_CRegs)` pattern and fill the superclass-owned tables in place, instead of
maintaining redundant instance-level arrays; this fixes black output on
`picture.datatype v39–v42`. Firmware tile heights from untrusted replies are now
validated against the allocated tile buffer via the new `tile_rows` field in
`ZZ9KPictureDatatypeTarget`, guarding all four tile writers. A missing overflow
check after `zz9k_picture_accumulate_surface_bytes` in the PNG full-image layout
path is added. The `WritePixelLine8` per-row buffer is now allocated rounded up
to a 16-pixel multiple as required by `graphics.library`.

On `picture.datatype v39-v42`, the class now decodes into a
legacy 8-bit remapped bitmap instead of aborting PNG decode. That OS3.1 fallback uses a
216-color palette, publishes a normal `PDTA_BitMap`, and degrades transparent
PNG alpha to a transparent palette index because the v43/v47 alpha contracts
are not available there.

Hardware validation passed with real transparent PNGs in MultiView and a
browser client. The alpha path is now the active validated DataType route, not a
diagnostic-only experiment.

The current source uses neutral v43 `PDTM_WRITEPIXELARRAY` helper names,
keeps the previously tried PNG surface experiments behind the disabled
`ZZ9K_PICTURE_ENABLE_PNG_ALPHA_EXPERIMENTS` guard, keeps the broad v47 direct
probe disabled, keeps the narrow JPEG RGB direct probe disabled after the
2.57-second `42.145` benchmark, and enables only the v43 write-pixel path plus
the OS3.1 bitmap fallback in the release candidate.

Both descriptors use the `zz9k-picture` base name, `pict` group, binary magic
masks for JPEG and PNG, and priority `10` for the eventual active path.
