# adgui-cpp – AVRDUDE GUI in C++ / Qt6

A faithful C++ / Qt6 port of the original Python/PySide6 GUI
(`src/python/adgui.py`) that ships with AVRDUDE ≥ 8.

---

## Files in this port

| C++ file | Origin (Python) | Purpose |
|---|---|---|
| `main.cpp` | entry point in `adgui.py` | `QApplication` + `MainWindow` |
| `mainwindow.{h,cpp}` | `class adgui` (main window) | Main window, menu bar, log view, progress bar |
| `devicedialog.{h,cpp}` | `adgui` device-selection section | Filters & lists AVR parts (`device.ui`) |
| `programmerdialog.{h,cpp}` | `adgui` programmer-selection section | Filters programmers, sets port (`programmer.ui`) |
| `memoriesdialog.{h,cpp}` | `adgui` memory-operations section | Flash/EEPROM/Fuse tabs (`memories.ui`) |
| `logleveldialog.{h,cpp}` | log-level dialog | Slider for verbosity (`loglevel.ui`) |
| `devinfodialog.{h,cpp}` | device-info dialog | Shows part info (`devinfo.ui`) |
| `helpdialog.{h,cpp}` | help dialog | Quick-start text (`help.ui`) |
| `CMakeLists.txt` | — | Qt6 + libavrdude build |

---

## Steps to port from Python/PySide6 to C++/Qt6

### 1. Reuse the `.ui` files as-is

The `.ui` files (`adgui.ui`, `device.ui`, `programmer.ui`, `memories.ui`,
`loglevel.ui`, `devinfo.ui`, `help.ui`, `about.ui`) are plain XML and are
**100 % compatible with Qt6's `uic` tool**.  Copy them from
`src/python/` into a `ui/` sub-directory of this project. CMake's `AUTOUIC`
will compile them to `ui_<name>.h` automatically.

```
cp avrdude/src/python/*.ui adgui-cpp/ui/
```

### 2. Replace `QUiLoader` with compile-time `uic`

| Python approach | C++ approach |
|---|---|
| `QUiLoader().load("adgui.ui")` at runtime | `#include "ui_adgui.h"` + `ui->setupUi(this)` at compile time |

In each dialog, declare `Ui::ClassName *ui;` (the generated class), call
`ui->setupUi(this)` in the constructor, and access widgets as `ui->widgetName`.

### 3. Translate signals & slots

Python lambdas and `connect()` calls map directly to Qt C++ `connect()`:

```python
# Python
self.btn.clicked.connect(lambda: self.do_something())
```
```cpp
// C++
connect(ui->btn, &QPushButton::clicked, this, &MyDialog::doSomething);
```

### 4. Replace the SWIG bindings with the libavrdude C API

The Python GUI uses `import swig_avrdude`.  In C++ you `#include <libavrdude.h>`
and call the same underlying functions directly:

| Python (SWIG) | C++ (direct) |
|---|---|
| `swig_avrdude.init_config()` | `avrdude_initconfig()` |
| `swig_avrdude.read_config(path)` | `read_config(path)` |
| `swig_avrdude.locate_part(db, id)` | `locate_part(db, id)` |
| `swig_avrdude.locate_programmer(db, id)` | `locate_programmer(db, id)` |
| `avr.open(pgm, part)` | `pgm->open(pgm, port)` |
| `avr_read_mem(pgm, part, mem, 0)` | `avr_read(pgm, part, mem, 0)` |
| `avr_write_mem(pgm, part, mem, …)` | `avr_write(pgm, part, mem, …)` |
| `fileio(…)` | `fileio(…)` |

All calls are made from **`AvrdudeWorker::run()`** (a `QThread` sub-class),
keeping the GUI thread free — the same reason the Python GUI used threads.

### 5. Progress and log callbacks

Register C callbacks with libavrdude and emit Qt signals back to the main thread:

```cpp
// In AvrdudeWorker::run():
avrdude_set_msg_callback([](int level, const char *fmt, va_list ap, void *ctx) {
    auto *w = static_cast<AvrdudeWorker *>(ctx);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    emit w->logMessage(level, QString::fromUtf8(buf));
}, this);

avrdude_set_progress_callback([](const char *task, int progress, void *ctx) {
    auto *w = static_cast<AvrdudeWorker *>(ctx);
    emit w->progress(QString::fromUtf8(task), progress);
}, this);
```

### 6. Settings persistence

Replace Python's `QSettings`-equivalent with `QSettings` in C++:

```cpp
QSettings s("avrdudes", "adgui");
s.setValue("device",     m_currentPart);
s.setValue("programmer", m_currentProgrammer);
s.setValue("port",       m_currentPort);
```

---

## Building

### Prerequisites

- CMake ≥ 3.20
- Qt 6.2+ (`Qt6::Widgets`)
- libavrdude (built from AVRDUDE ≥ 8 source)
- A C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+)

### Linux / macOS

```bash
# Copy .ui files from the AVRDUDE source tree
cp /path/to/avrdude/src/python/*.ui ui/

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

If Qt6 is not on PATH:
```bash
cmake .. -DQt6_DIR=/opt/Qt/6.7.0/gcc_64/lib/cmake/Qt6
```

If libavrdude is not found by pkg-config:
```bash
cmake .. \
  -DAVRDUDE_INCLUDE_DIRS=/usr/local/include \
  -DAVRDUDE_LIBRARY_DIRS=/usr/local/lib \
  -DAVRDUDE_LIBRARIES=avrdude
```

### Windows (MSVC + vcpkg)

```bat
vcpkg install qt6-base
cmake -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
```

---

## Hooking in the real libavrdude calls

Look for the `// TODO` stubs in `mainwindow.cpp` (`AvrdudeWorker::run()`)
and replace each `case` block with the corresponding libavrdude C API calls.
The `HAVE_LIBAVRDUDE` preprocessor macro is defined by CMake when the library
is found, so you can guard real calls with `#ifdef HAVE_LIBAVRDUDE`.
