# ZZ9000 Picture DataType

Copyright (C) 2024-2026, Dimitris Panokostas / BlitterStudio

`zz9k-picture.datatype` is packaged as a side-by-side subclass of the system
`picture.datatype`. It must not replace `Classes/DataTypes/picture.datatype`.

The class binary installs as:

```text
Classes/DataTypes/zz9k-picture.datatype
```

The JPEG and PNG recognition descriptors are currently packaged inactive under:

```text
Storage/DataTypes/ZZ9000-JPEG
Storage/DataTypes/ZZ9000-JPEG.info
Storage/DataTypes/ZZ9000-PNG
Storage/DataTypes/ZZ9000-PNG.info
```

For hardware testing, install the class and temporarily activate the packaged
descriptors:

```text
copy Classes/DataTypes/zz9k-picture.datatype TO SYS:Classes/DataTypes/
copy Storage/DataTypes/ZZ9000-JPEG#? TO DEVS:DataTypes/
copy Storage/DataTypes/ZZ9000-PNG#? TO DEVS:DataTypes/
AddDataTypes DEVS:DataTypes/ZZ9000-JPEG
AddDataTypes DEVS:DataTypes/ZZ9000-PNG
AddDataTypes LIST
```

`AddDataTypes LIST` should show `ZZ9000-JPEG` and `ZZ9000-PNG` before
MultiView is expected to route matching files to `zz9k-picture.datatype`.
Until hardware validation is finished, keeping them in `Storage:DataTypes`
avoids routing normal JPEG/PNG application loads to the unvalidated class path.

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

Current PNG alpha handling deliberately keeps the v43 picture object contract
opaque while the OS3/v47 true-alpha subclass contract is still under
investigation. For the current release-candidate build, alpha-containing PNGs are
forced back through the RGB888 tile path after clearing the visible alpha
contract. This is the known stable path, but it can expose hidden RGB from
fully transparent source pixels; it is not a final solution for browser
page-background transparency.

Hardware testing confirmed the `42.135` rollback no longer crashes, and the
follow-up benchmark run showed no regressions. The failed `42.133`/`42.134`
experiments show that requesting RGBA8888 firmware tiles and then writing RGB
rows through an otherwise opaque v43 object is not a safe compatibility path.
The next transparent-PNG attempt should either wait for a confirmed OS3/v47
alpha contract or add a firmware/API path that supplies RGB and transparency
data without asking the current opaque v43 datatype route to process RGBA
tiles.

The current source uses neutral v43 `PDTM_WRITEPIXELARRAY` helper names and
keeps the previously tried true-alpha/direct-buffer branches behind the disabled
`ZZ9K_PICTURE_ENABLE_PNG_ALPHA_EXPERIMENTS` guard. Those branches are retained
only as local experiment scaffolding; they are not active in the release
candidate.

Both descriptors use the `zz9k-picture` base name, `pict` group, binary magic
masks for JPEG and PNG, and priority `10` for the eventual active path.
