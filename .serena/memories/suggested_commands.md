# Useful commands
- Core: `cmake -S . -B <build> -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DNYANFI_BUILD_TESTS=ON -DNYANFI_BUILD_GUI=OFF`, then `cmake --build <build>`.
- Tests: `ctest --test-dir <build> --output-on-failure`; run `<build>/tests/core_tests.exe` for doctest totals.
- GUI: same toolchain with `-DNYANFI_BUILD_GUI=ON -DNYANFI_BUILD_TESTS=OFF -DWX_CONFIG=<wx-prefix>/bin/wx-config`, then `cmake --build <build> --target nyanfi`.
- Checks: `python3 scripts/check_commands.py`, `python3 scripts/check_literals.py`, `git diff --check`.