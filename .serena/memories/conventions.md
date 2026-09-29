# Code conventions
- Tabs indent code; Japanese comments/UI text are common.
- wx-independent logic is namespaced in `gui/*.h/.cpp` and must not include wx headers; wx dialog classes stay in anonymous namespaces and expose a `Run(...)` function.
- Use `UnicodeString`/`UsrIniFile` for VCL-compatible state. Existing `gui/history.*` owns edit/view MRU state and ini persistence; dialogs should reuse it rather than duplicate storage.
- New source files use include guards such as `NYANFI_GUI_<NAME>_H`; tests use doctest and are discovered by `tests/CMakeLists.txt` glob.