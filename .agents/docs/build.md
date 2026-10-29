# Build & tests

Out-of-source build is required (in-source is blocked).

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/azeroth-server -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DSCRIPTS=static -DMODULES=static
make -j$(nproc) && make install
```

C++20 required (`CMAKE_CXX_STANDARD 20`). Useful flags: `BUILD_TESTING=ON` (Google Test), `NOPCH=1` (disable precompiled headers). Full set in `conf/dist/config.cmake`. `compile_commands.json` is exported automatically.

Tests (Google Test, in `src/test/`): configure `-DBUILD_TESTING=ON`, then `ctest` or `./src/test/unit_tests` from the build dir.

## Windows (local toolchain)

This checkout also builds on Windows with the local toolchain listed in `AGENTS.md`. The facts below were verified on the development machine; keep them in mind instead of re-deriving them.

- The configured tree is `build-xs-vs2022`: generator `Visual Studio 17 2022`, `CMAKE_GENERATOR_INSTANCE=E:/Vs2022-IDE`, `BOOST_ROOT=E:/boost_1_78_0`, MySQL and OpenSSL under their `E:\` installs, `SCRIPTS=static`, `MODULES=static`. Reuse it; do not configure a second tree for the same checkout.
- `cmake --version` must report **3.25.3** to match `build-xs-vs2022/CMakeCache.txt` (`CMAKE_CACHE_MAJOR/MINOR/PATCH`). On this machine the executable on `PATH` is `D:\bin\cmake.exe`; `E:\CMake3.25.3\bin` contains no `cmake.exe`, so that path cannot be used as the binary.
- MSVC is not on `PATH`. Either run builds from a shell after `call E:\Vs2022-IDE\VC\Auxiliary\Build\vcvars64.bat`, or let CMake invoke MSBuild as below. Scripts that call `cl` directly need that `vcvars64.bat` first.
- Only `RelWithDebInfo` is used, per `AGENTS.md`.

```powershell
cmake --build build-xs-vs2022 --config RelWithDebInfo --target worldserver --parallel 8
cmake --build build-xs-vs2022 --config RelWithDebInfo --target authserver --parallel 8
```

- Outputs land in `build-xs-vs2022\bin\RelWithDebInfo\`.
- The tree is bound to one worktree through `CMAKE_HOME_DIRECTORY`. A second worktree of the same repository needs its own build directory; reusing this one compiles the wrong sources.
- `src/test/` is not built by default; enabling `BUILD_TESTING` needs a fresh configure.
