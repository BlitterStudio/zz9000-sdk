# DataType Release Activation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the hardware-passed `zz9k-picture.datatype 42.139` path release-ready in package docs and guard tests without changing rendering behavior.

**Architecture:** Keep descriptors packaged inactive under `Storage/DataTypes`, document the opt-in activation steps clearly, and guard the wording with focused C source tests. No rendering code or descriptor matching behavior changes in this slice.

**Tech Stack:** CMake/CTest, C source guard tests, Markdown docs, existing PowerShell/POSIX package scripts.

---

## File Structure

- Modify `CMakeLists.txt`: add one small doc-guard test target near the existing DataType tests.
- Create `tests/picture_datatype_doc_test.c`: source guard for `docs/zz9k-picture-datatype.md`.
- Modify `docs/zz9k-picture-datatype.md`: replace pre-validation language with validated opt-in activation language.
- Modify `tests/release_smoke_doc_test.c`: tighten expectations for active DataType smoke setup.
- Modify `docs/zz9k-release-smoke.md`: include descriptor activation commands before MultiView/DataType client checks.
- Modify `README.md`: add a short package note that the class is shipped and descriptors are opt-in from `Storage/DataTypes`.
- Do not modify package scripts unless `package_script_test` shows the existing `Storage/DataTypes` placement guard is wrong.

---

### Task 1: Add Picture DataType Doc Guard

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/picture_datatype_doc_test.c`
- Test: `picture_datatype_doc_test`

- [ ] **Step 1: Add the new CMake test target**

Add this block near the existing DataType tests in `CMakeLists.txt`, after `picture_datatype_source_test`:

```cmake
add_executable(picture_datatype_doc_test
  tests/picture_datatype_doc_test.c
)
add_test(
  NAME picture_datatype_doc_test
  COMMAND picture_datatype_doc_test
          ${CMAKE_CURRENT_SOURCE_DIR}/docs/zz9k-picture-datatype.md
)
```

- [ ] **Step 2: Write the failing doc guard**

Create `tests/picture_datatype_doc_test.c`:

```c
/*
 * Source guard for the packaged picture DataType documentation.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path)
{
  FILE *file;
  long length;
  char *data;

  file = fopen(path, "rb");
  if (!file) return 0;
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return 0;
  }
  length = ftell(file);
  if (length < 0) {
    fclose(file);
    return 0;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return 0;
  }
  data = (char *)malloc((size_t)length + 1U);
  if (!data) {
    fclose(file);
    return 0;
  }
  if (fread(data, 1U, (size_t)length, file) != (size_t)length) {
    free(data);
    fclose(file);
    return 0;
  }
  data[length] = '\0';
  fclose(file);
  return data;
}

static int expect_contains(const char *source, const char *needle)
{
  if (strstr(source, needle)) return 1;
  printf("missing %s\n", needle);
  return 0;
}

int main(int argc, char **argv)
{
  char *source;
  int ok;

  if (argc != 2) {
    printf("usage: %s <docs/zz9k-picture-datatype.md>\n", argv[0]);
    return 2;
  }
  source = read_file(argv[1]);
  if (!source) {
    printf("failed to read %s\n", argv[1]);
    return 2;
  }

  ok = 1;
  ok &= expect_contains(source, "zz9k-picture.datatype 42.139");
  ok &= expect_contains(source, "validated SDK v2 DataType candidate");
  ok &= expect_contains(source, "packaged inactive under `Storage/DataTypes`");
  ok &= expect_contains(source, "copy Storage/DataTypes/ZZ9000-JPEG#? TO DEVS:DataTypes/");
  ok &= expect_contains(source, "copy Storage/DataTypes/ZZ9000-PNG#? TO DEVS:DataTypes/");
  ok &= expect_contains(source, "AddDataTypes DEVS:DataTypes/ZZ9000-JPEG");
  ok &= expect_contains(source, "AddDataTypes DEVS:DataTypes/ZZ9000-PNG");
  ok &= expect_contains(source, "AddDataTypes LIST");

  free(source);
  return ok ? 0 : 1;
}
```

- [ ] **Step 3: Build and run to verify RED**

Run:

```powershell
cmake --build build\native-msvc --config Debug --target picture_datatype_doc_test
ctest --test-dir build\native-msvc -C Debug -R picture_datatype_doc_test --output-on-failure
```

Expected: build succeeds, test fails with at least one missing wording string such as `validated SDK v2 DataType candidate`.

---

### Task 2: Update Picture DataType Documentation

**Files:**
- Modify: `docs/zz9k-picture-datatype.md`
- Test: `picture_datatype_doc_test`

- [ ] **Step 1: Replace pre-validation language**

In `docs/zz9k-picture-datatype.md`, replace the opening install/activation section with wording that keeps the package layout inactive by default and documents the validated candidate:

```markdown
`zz9k-picture.datatype 42.139` is the validated SDK v2 DataType candidate for
the current package. It is packaged as a side-by-side subclass of the system
`picture.datatype` and must not replace `Classes/DataTypes/picture.datatype`.
```

Keep the class path block:

```text
Classes/DataTypes/zz9k-picture.datatype
```

Replace the descriptor introduction with:

```markdown
The JPEG and PNG recognition descriptors are packaged inactive under
`Storage/DataTypes` so activation is an explicit install step:
```

Keep the existing descriptor path block.

- [ ] **Step 2: Update activation guidance**

Replace the current "For hardware testing" paragraph with:

````markdown
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
````

- [ ] **Step 3: Run GREEN**

Run:

```powershell
cmake --build build\native-msvc --config Debug --target picture_datatype_doc_test
ctest --test-dir build\native-msvc -C Debug -R picture_datatype_doc_test --output-on-failure
```

Expected: `picture_datatype_doc_test` passes.

---

### Task 3: Tighten Release Smoke Checklist Guard

**Files:**
- Modify: `tests/release_smoke_doc_test.c`
- Test: `release_smoke_doc_test`

- [ ] **Step 1: Add failing expectations**

In `tests/release_smoke_doc_test.c`, after the existing `zz9k-dtprobe` / `MultiView` expectations, add:

```c
  ok &= expect_contains(source, "copy Storage/DataTypes/ZZ9000-JPEG#? TO DEVS:DataTypes/");
  ok &= expect_contains(source, "copy Storage/DataTypes/ZZ9000-PNG#? TO DEVS:DataTypes/");
  ok &= expect_contains(source, "AddDataTypes DEVS:DataTypes/ZZ9000-JPEG");
  ok &= expect_contains(source, "AddDataTypes DEVS:DataTypes/ZZ9000-PNG");
  ok &= expect_contains(source, "AddDataTypes LIST");
  ok &= expect_contains(source, "DataType descriptors are activated from `Storage/DataTypes`");
```

- [ ] **Step 2: Build and run to verify RED**

Run:

```powershell
cmake --build build\native-msvc --config Debug --target release_smoke_doc_test
ctest --test-dir build\native-msvc -C Debug -R release_smoke_doc_test --output-on-failure
```

Expected: test fails with missing activation wording.

---

### Task 4: Update Release Smoke Checklist

**Files:**
- Modify: `docs/zz9k-release-smoke.md`
- Test: `release_smoke_doc_test`

- [ ] **Step 1: Add activation commands before DataType client checks**

In the `Image, Viewer, And DataTypes` section of `docs/zz9k-release-smoke.md`, insert this block before the `zz9k-jpeg` command block:

````markdown
If the package was installed with descriptors still inactive, activate the
validated DataType descriptors before the MultiView/browser checks:

```text
copy Storage/DataTypes/ZZ9000-JPEG#? TO DEVS:DataTypes/
copy Storage/DataTypes/ZZ9000-PNG#? TO DEVS:DataTypes/
AddDataTypes DEVS:DataTypes/ZZ9000-JPEG
AddDataTypes DEVS:DataTypes/ZZ9000-PNG
AddDataTypes LIST
```
````

- [ ] **Step 2: Add explicit expected signal**

In the expected pass signal list, add:

```markdown
- DataType descriptors are activated from `Storage/DataTypes`, and
  `AddDataTypes LIST` shows `ZZ9000-JPEG` and `ZZ9000-PNG`.
```

- [ ] **Step 3: Run GREEN**

Run:

```powershell
cmake --build build\native-msvc --config Debug --target release_smoke_doc_test
ctest --test-dir build\native-msvc -C Debug -R release_smoke_doc_test --output-on-failure
```

Expected: `release_smoke_doc_test` passes.

---

### Task 5: Add Top-Level Package Note

**Files:**
- Modify: `README.md`
- Test: focused docs/package tests

- [ ] **Step 1: Update package contents list**

In `README.md`, under the package contents list, add a bullet after the CLI tools bullet:

```markdown
- `Classes/DataTypes/zz9k-picture.datatype` plus JPEG/PNG descriptors packaged
  inactive under `Storage/DataTypes` for explicit opt-in activation
```

- [ ] **Step 2: Run focused tests**

Run:

```powershell
ctest --test-dir build\native-msvc -C Debug -R "picture_datatype_doc_test|release_smoke_doc_test|package_script_test" --output-on-failure
```

Expected: all selected tests pass.

---

### Task 6: Full Verification And Commit

**Files:**
- Verify all modified files
- Commit all implementation changes

- [ ] **Step 1: Run whitespace check**

Run:

```powershell
git diff --check
```

Expected: exit code 0. CRLF conversion warnings are acceptable if there are no whitespace errors.

- [ ] **Step 2: Run full native build**

Run:

```powershell
cmake --build build\native-msvc --config Debug
```

Expected: exit code 0.

- [ ] **Step 3: Run full native CTest**

Run:

```powershell
ctest --test-dir build\native-msvc -C Debug --output-on-failure
```

Expected: 100% tests passed.

- [ ] **Step 4: Run AmigaOS 3 Docker build**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-m68k-amigaos.ps1
```

Expected: exit code 0.

- [ ] **Step 5: Run AmigaOS 3 package script**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package-m68k-amigaos.ps1
```

Expected: prints `Packaged AmigaOS 3 SDK to ...\build\package\amigaos3`.

- [ ] **Step 6: Confirm package docs**

Run:

```powershell
Select-String -LiteralPath build\package\amigaos3\Docs\zz9k-picture-datatype.md -Pattern "validated SDK v2 DataType candidate"
Select-String -LiteralPath build\package\amigaos3\Docs\zz9k-release-smoke.md -Pattern "DataType descriptors are activated from"
```

Expected: both commands print matching lines.

- [ ] **Step 7: Commit**

Run:

```powershell
git add README.md docs\zz9k-picture-datatype.md docs\zz9k-release-smoke.md tests\picture_datatype_doc_test.c tests\release_smoke_doc_test.c CMakeLists.txt
git commit -m "Document DataType release activation"
```

Expected: commit succeeds with no attribution trailers.
