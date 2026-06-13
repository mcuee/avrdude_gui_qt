# Windows Packaging Guide

This document covers everything needed to produce a self-contained
`AVRDUDE-GUI-x.y.z-win64-setup.exe` installer for end users.

---

## Overview of the pipeline

```
cmake --build  →  windeployqt6  →  Inno Setup 6  →  setup.exe
      (adgui.exe)    (stage Qt DLLs)   (single-file
                                         installer)
```

---

## Prerequisites

| Tool | Where to get it | Notes |
|---|---|---|
| CMake 3.20+ | cmake.org | Add to PATH |
| MSVC 2022 (x64) | Visual Studio Installer → "Desktop development with C++" | Or use the free Build Tools |
| Qt 6.2+ (MSVC x64) | qt.io / Qt Maintenance Tool | `windeployqt6.exe` ships with it |
| Inno Setup 6 | jrsoftware.org/isinfo.php | Default install path is fine |
| (Optional) libavrdude | Built from AVRDUDE ≥ 8 source | Place `libavrdude.dll` beside `adgui.exe` |

---

## Step-by-step (manual)

### 1. Build the application

Open a **x64 Native Tools Command Prompt for VS 2022**, then:

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
      -DQt6_DIR="C:\Qt\6.7.3\msvc2022_64\lib\cmake\Qt6"
cmake --build build --config Release --parallel
```

The executable lands at `build\Release\adgui.exe`.

### 2. Stage the deployment directory with `windeployqt6`

`windeployqt6` inspects the EXE, finds all Qt modules it uses, and copies
the required DLLs, plugins, and translations into a single directory.

```bat
mkdir build\deploy
copy build\Release\adgui.exe build\deploy\

C:\Qt\6.7.3\msvc2022_64\bin\windeployqt6.exe ^
    --dir build\deploy ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    --no-virtualkeyboard ^
    --no-quick-import ^
    --translations en ^
    build\deploy\adgui.exe
```

After this step `build\deploy\` contains a fully self-contained application
tree. You can run `build\deploy\adgui.exe` directly with no Qt installation.

**Typical directory layout produced by windeployqt6:**

```
build\deploy\
├── adgui.exe
├── libavrdude.dll          ← copy manually if using shared libavrdude
├── Qt6Core.dll
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── platforms\
│   └── qwindows.dll        ← required – without this the GUI won't start
├── styles\
│   └── qwindowsvistastyle.dll
├── imageformats\
│   ├── qico.dll
│   └── qjpeg.dll
├── translations\
│   └── qt_en.qm
└── vc_redist.x64.exe       ← staged by windeployqt if MSVC runtime needed
```

> **Common mistake:** forgetting to copy `libavrdude.dll` into the deploy
> directory.  The executable will fail silently at startup because Windows
> cannot locate the DLL.  Use `Dependencies` (github.com/lucasg/Dependencies)
> to check for missing DLLs before packaging.

### 3. (Optional) copy `avrdude.conf`

AVRDUDE needs `avrdude.conf` to know about devices and programmers.
If you ship a bundled config, copy it next to the EXE:

```bat
copy avrdude.conf build\deploy\
```

The GUI loads it with:
```cpp
QCoreApplication::applicationDirPath() + "/avrdude.conf"
```

### 4. Compile the Inno Setup installer

```bat
mkdir dist
"C:\Program Files (x86)\Inno Setup 6\iscc.exe" /Odist package\windows.iss
```

This produces `dist\AVRDUDE-GUI-1.0.0-win64-setup.exe`.

#### Key sections of `package/windows.iss`

| Section | Purpose |
|---|---|
| `[Setup]` | App metadata, compression (lzma2/ultra64), architecture guard (x64), minimum Windows version (10.0.17763 = 1809) |
| `[Files]` | Recursive copy of `build\deploy\*` into `{app}` |
| `[Icons]` | Start Menu + optional Desktop shortcut |
| `[Tasks]` | User-visible checkboxes (desktop icon on/off) |
| `[Registry]` | Writes `HKCU\Software\avrdudes\AVRDUDE GUI\InstallPath` for other tools to discover |
| `[Run]` | "Launch AVRDUDE GUI" checkbox on the final installer page |
| `[UninstallDelete]` | Removes `%APPDATA%\avrdudes\adgui.ini` (QSettings file) on uninstall |
| `[Code]` | Pascal script stub for VC++ runtime presence check |

#### Customising the installer icon

Place a 256×256 `adgui.ico` in `package\` and uncomment these lines in
`windows.iss`:

```ini
SetupIconFile    = package\adgui.ico
UninstallDisplayIcon = {app}\adgui.exe
```

### 5. One-shot helper script

The batch file `package\build_and_package.bat` runs all four steps above:

```bat
package\build_and_package.bat Release "C:\Qt\6.7.3\msvc2022_64"
```

---

## Automated builds with GitHub Actions

`.github/workflows/windows.yml` runs the full pipeline on every push and
pull request:

1. **jurplel/install-qt-action** – downloads and caches Qt 6.
2. `cmake` configure + build.
3. `windeployqt6` staging.
4. `iscc` Inno Setup compile.
5. Uploads the installer as a workflow artifact (retained 30 days).
6. On `v*` tags, attaches the installer to the GitHub Release automatically.

Trigger a release by pushing a tag:
```bash
git tag v1.0.0
git push origin v1.0.0
```

---

## Troubleshooting

### "The application was unable to start correctly (0xc000007b)"
64-bit EXE loaded a 32-bit DLL (or vice versa).  Make sure Qt, MSVC
toolchain, and libavrdude are all the same architecture (x64).

### "Entry point not found" on Qt6Core.dll
The DLL in `deploy\` is from a different Qt version than the one used to
build.  Re-run `windeployqt6` pointing to the correct Qt installation.

### GUI starts but shows no window (black screen)
`platforms\qwindows.dll` is missing.  `windeployqt6` should stage it
automatically, but double-check it is present in `deploy\platforms\`.

### libavrdude functions not found at runtime
`libavrdude.dll` is not in the deploy directory or in the system PATH.
Copy it manually alongside `adgui.exe`.

### Inno Setup error "Source file does not exist"
`build\deploy\` is empty or the path in `#define DeployDir` is wrong.
Run `windeployqt6` first (Step 2 above) and verify the path matches.
