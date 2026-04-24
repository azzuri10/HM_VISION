# HM_VISION Project Structure

## Canonical layout (`include` + `src`)

All application source and public headers live under:

- `include/<Module>/` — headers included as `"Module/...h"` (with `include/` on the compiler include path).
- `src/<Module>/` — implementation files (`.cpp`).

`VisionSystem/` 根目录除 **`main.cpp`** 外，不再保留与上述模块平行的另一套 `Device/`、`Common/` 等旧目录，避免同模块多路径重复。

## Migration status (this repository)

**Completed:** modules are consolidated under `include/` + `src/`, including:

- `UI`, `Core`, `Communication`, `Data`, `Algorithm`
- `Device` — `ICamera`, camera manager/factory, USB/GigE 摄像头、`ModbusPLC`、PLC 接口等
- `Common` — 例如 `Types.h`、算法 DTO、相机类型等（以 `include/Common/` 为准）

## Build artifacts (do not commit)

- `build/`, `build_qt/`, `build_*/`, `out/`, `cmake-build-*/`

Use CMake presets or Qt Creator build directories locally only.

## Tests

- `tests/` — executables registered with CTest
- `HMVISION_BUILD_TESTS` — set `OFF` to skip the test subproject

Qt Creator 使用说明见仓库内 `QtCreator打开说明.txt`。
