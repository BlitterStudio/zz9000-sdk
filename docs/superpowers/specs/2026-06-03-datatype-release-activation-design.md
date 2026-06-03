# DataType Release Activation Design

Date: 2026-06-03

## Context

`zz9k-picture.datatype 42.139` has now passed the key hardware checks for the
current release candidate:

- reference diagnostic mode displays through the stable v43
  `PDTM_WRITEPIXELARRAY` path
- normal JPEG and PNG DataType loads route decoded pixels through the same v43
  RGB helper path
- the broader package-level runtime smoke matrix was reported as passing

The package still installs the JPEG and PNG descriptors under
`Storage/DataTypes`, and the documentation still describes activation as a
temporary hardware-test step. That was correct before the hardware pass, but it
now leaves the validated release path looking experimental.

## Goal

Make the validated v43 DataType path release-ready from the SDK package point of
view without changing rendering behavior.

The next implementation should update release docs and package checks so users
can clearly install or activate the DataType class and descriptors, while still
leaving the actual descriptors in `Storage/DataTypes` by default unless the
release owner decides to ship them active under `DEVS:DataTypes`.

## Non-Goals

- Do not enable direct ARM framebuffer/DataType rendering by default.
- Do not re-enable PNG alpha or v47 direct-buffer experiments.
- Do not move descriptors from `Storage/DataTypes` to an active package
  location unless that becomes an explicit release decision.
- Do not change descriptor priority, magic matching, or runtime decode paths in
  this slice.

## Proposed Design

Keep the package layout conservative, but update the release-facing contract:

1. Document `zz9k-picture.datatype 42.139` as the validated SDK v2 DataType
   candidate for the current package.
2. Replace "until hardware validation is finished" language with explicit
   activation guidance: descriptors are packaged in `Storage/DataTypes` by
   default so users can opt in by copying them to `DEVS:DataTypes` and running
   `AddDataTypes`.
3. Add release-smoke wording that the active DataType check requires the
   descriptors to be activated first.
4. Add or tighten source/package guards so the docs, package scripts, and
   descriptor placement stay aligned.

This keeps the release behavior safe for existing systems while making the
validated opt-in path clear and testable.

## Files

Expected implementation touch points:

- `docs/zz9k-picture-datatype.md`
- `docs/zz9k-release-smoke.md`
- `README.md` if the top-level package description needs a shorter note
- `tests/package_script_test.c`
- `tests/release_smoke_doc_test.c`
- `tests/picture_datatype_source_test.c` only if the version/marker guard needs
  a wording update

Package scripts should only change if a test shows they do not already preserve
the intended `Storage/DataTypes` placement.

## Testing

Use TDD for any changed guard:

1. Add or tighten one focused test expectation.
2. Run the focused test and confirm it fails for the expected missing wording or
   package rule.
3. Update the docs or package rule.
4. Re-run the focused test and then the normal verification set.

Expected verification:

- focused doc/package tests
- `git diff --check`
- native MSVC build
- native CTest
- AmigaOS 3 Docker build
- AmigaOS 3 package script

## Risks

The main risk is implying that descriptors are active by default when the package
still places them in `Storage/DataTypes`. The wording must be explicit: the
class is packaged, the descriptors are packaged inactive, and activation is an
intentional user or release-install step.

The other risk is reopening direct rendering work under a release-polish label.
This slice should not change rendering code.
