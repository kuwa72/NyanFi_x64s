# NyanFi_x64s project map
- Windows-only C++ port of NyanFi. VCL reference sources remain under `src/`; `gui/` is the wxWidgets port.
- `nyanfi_core` is built from Phase 0 sources; wx-independent GUI logic is `nyanfi_gui_core`; wx adapters and `nyanfi` are in `gui/CMakeLists.txt`.
- MainFrame dispatches command names that must exist in `src/usr_cmdlist.cpp`; do not invent command names.
- Dialog migration documentation should cite measured VCL file/line locations and explicitly list `未移植 (未実装扱い)` behavior.