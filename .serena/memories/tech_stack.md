# Stack and build
- C++20, CMake >=3.24, Ninja, doctest.
- Windows target is MinGW-w64 GCC; Linux-host cross builds use `cmake/toolchain-mingw-w64.cmake`.
- GUI is wxWidgets 3.3.x MSW; configure with `-DNYANFI_BUILD_GUI=ON -DWX_CONFIG=<prefix>/bin/wx-config` and build target `nyanfi`.
- Core tests can be built with `NYANFI_BUILD_GUI=OFF`; `nyanfi_gui_core` remains available for pure GUI logic tests.