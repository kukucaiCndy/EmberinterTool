# AGENTS.md - EmberInterDebugTool (尘智串口调试工具)

## Quick build

```bash
# Linux (Debian/Ubuntu) — release build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
# Output: build/bin/serial-monitor, build/bin/serial-monitor-cli

# Debug build (with tests)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build . -j$(nproc)

# Windows (MSYS2 MINGW64) — must use MinGW Makefiles; ensure mingw-w64-x86_64-make is installed
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/mingw64
mingw32-make -j$(nproc)
```

Lint/typecheck: there is no standalone lint step. Rely on compile warnings. CMake sets `CMAKE_CXX_STANDARD 17` with `-Wall -Wextra` convention (per `.trae/rules/project.md`).

## Architecture

3-layer C++17/Qt5 build:

```
src/core/        → libserialmonitor_core.a (static lib)
src/gui/         → serial-monitor executable (GUI + IPC server)
src/cli/         → serial-monitor-cli executable (IPC client)
```

- **SerialEngine** (`src/core/serial_engine.h`) runs in a dedicated QThread. Signal/slot connections to the engine must respect thread boundaries — use queued connections when crossing threads.
- CMake enables `AUTOMOC`, `AUTORCC`, `AUTOUIC` globally, so no need to manually call moc/uic/rcc.
- `resources/resources.qrc` is the Qt resource file; styles and icons live under `resources/`.
- Config persisted to `QStandardPaths::AppDataLocation/config.json` (~/.config/EmberInter/serial-monitor/config.json on Linux).

## IPC (GUI ↔ CLI)

- CLI does **not** open serial ports directly. It communicates with the GUI via QLocalSocket (name: `serial_monitor_ipc`).
- **The GUI must be running first** for any CLI operation to succeed.
- Protocol: newline-delimited JSON. Core lib uses `QJsonDocument`; CLI uses rapidjson (CLI avoids linking Qt heavy modules).
- See `docs/IPC通信协议文档.md` for full message schema.

## After changing version

Update version in all three places:
1. `CMakeLists.txt:2` — `project(... VERSION x.y.z)`
2. `deploy/emberInter.iss:6` — `#define MyAppVersion "x.y.z"`
3. `git tag vx.y.z`

Also regenerate the deb: `bash deb_pack/build_deb.sh` (requires build artifacts in `build/bin/`).

## Code conventions

- Header guards: `#ifndef HEADER_H` / `#define HEADER_H` (not `#pragma once`).
- `ConfigManager` is a singleton via `instance()`.
- GUI entry: `src/gui/main.cpp`. CLI entry: `src/cli/main.cpp`.
- `serial_monitor.py` at repo root is a **legacy standalone Python tool** — not part of the C++ build and not maintained in sync with the C++ project. Do not modify it as part of C++ work.

## Testing

- Tests directory `tests/` is referenced in CMakeLists.txt but currently **empty** — no tests exist.
- GoogleTest is optionally found at configure time; build test support with `-DBUILD_TESTS=ON`.

## Planned but not yet implemented

`.trae/rules/project.md` describes a multi-phase TabType/TabPage abstraction (Serial, SSH, CMD, STLink, JLink tabs). Currently the GUI uses a single global `LogView`, `SendPanel`, and `LogBuffer` in MainWindow. New features should follow the existing `TabInfo`-based structure in `SerialTabWidget` until the abstraction is actually built.

## Key docs

- `docs/IPC通信协议文档.md` — IPC message protocol specification
- `docs/CLI接口规范文档.md` — CLI interface specification
- `docs/项目架构与模块设计文档.md` — architecture and module design
- `.trae/rules/project.md` — project rules and planned architecture
- `project.md` — build/packaging/release instructions
