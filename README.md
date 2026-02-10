# giha

`giha` is a playground for experimenting with dart-based geometry processing on [Slang](https://github.com/shader-slang/slang).
The project focuses on implementing cyclic permutations of connectivity representations for geometry processing workflows.

## Features

- **Dart-based combinatorial model** 
    - A relaxed-condition half-edge structure allowing multiple edges (`include/giha/geometry/dart.h`).
    - Converts sparse face-vertex incidence lists into a canonical dart structure via a SpGEMM-like stitch phase (`include/giha/geometry`).
- **Reference kernels** — Runnable compute-kernel geometry-processing algorithms.
  - [x] Dart-based structure for connectivity and induced maps
  - [x] Orbit collector topology query
  - [x] Loop-like subdivision
  - [ ] Delaunay triangulation pass
  - [ ] Spatial acceleration construction and its query (e.g., BVH, KD-tree)

## Repository layout

```
include/            # Public C++ headers (geometry, linalg, slang helpers)
module/             # Slang modules and kernels
src/                # Host-side runtime and Slang helpers
test/               # Optional tests (enable with GIHA_TEST)
cmake/              # Helper scripts (e.g., DownloadGithubRelease.cmake)
```

Key building blocks:

- `module/giha/geometry/dart.slang` mirrors the dart map on the GPU and adds mutable/atomic variants suited for lock-free edits.
- `module/giha_kernel`, … Compute kernels for geometry processing against the combinatorial model.
- `include/giha/slang` hosts the slang compile binder.
- `include/giha/slang/adapters` hosts descriptors that bind dart buffers into RHI pipelines (easy to use or for test).

## Build requirements

- CMake ≥ 3.25
- A C++20 compiler (MSVC, clang, or gcc)
- Optional: GPU + [Slang](https://github.com/shader-slang/slang) for shader compilation (downloaded automatically when `GIHA_SLANG=ON`, the default).
- Optional: [slang-rhi](https://github.com/shader-slang/slang-rhi) (fetched automatically when `GIHA_SLANG_RHI=ON`).

## Configure & build

```bash
# 1. Configure (defaults: GIHA_SLANG=ON, GIHA_SLANG_RHI=OFF, GIHA_TEST=OFF)
cmake -S . -B build \
  -DGIHA_SLANG=ON \
  -DGIHA_SLANG_RHI=OFF \
  -DGIHA_TEST=OFF

# 2. Build
cmake --build build
```

Useful options:

| Option | Default | Description |
| ------ | ------- | ----------- |
| `GIHA_SLANG` | `ON` | Enable Slang support and download the matching SDK. |
| `GIHA_SLANG_RHI` | `OFF` | Build slang-rhi adapters/kernels (downloads slang-rhi). |
| `GIHA_TEST` | `OFF` | Build the test suite in `test/`. |
| `GIHA_LOG_LEVEL` | `3` | Compile-time log verbosity (`0` silent … `3` debug). |

Set options at configure time, e.g. `-DGIHA_TEST=ON`.

## Tests

```bash
cmake -S . -B build -DGIHA_TEST=ON
cmake --build build
cmake --build build --target test   # or: ctest --test-dir build
```

Tests rely on slang-rhi, so enabling them forces `GIHA_SLANG_RHI=ON` inside `test/CMakeLists.txt`.

## Running kernels

1. Build with `GIHA_SLANG_RHI=ON` so the slang-rhi runtime is available.
2. Use the adapters in `include/giha/geometry/adapters/slang-rhi/` to upload a `giha::DartMap` into GPU buffers and bind them into a shader cursor.
3. Compile the desired kernels from `module/giha_kernel/*.slang` (e.g., orbit collectors or subdivision) using `giha::kernel` helpers (`include/giha/slang/slang.h`).
4. Dispatch through slang-rhi’s `ComputeKernel` wrapper (`src/slang/adapters/slang-rhi.cpp`).

See `module/giha_kernel/subdivide.slang` for a complete in-shader example of inserting darts, connecting orbits, and updating induced vertex keys.

## References

- [Slang](https://github.com/shader-slang/slang) — shading language & compiler used for GPU kernels.
- [slang-rhi](https://github.com/shader-slang/slang-rhi) — rendering hardware interface leveraged by the runtime adapters.

## License

Distributed under the MIT License; see `LICENSE` for details.
