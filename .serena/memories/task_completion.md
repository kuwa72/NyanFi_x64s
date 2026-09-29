# Completion checklist
- Review `git status`/diff and never stage generated build or agent files.
- Run affected core configure/build, ctest, and direct doctest executable; run GUI cross configure/build target `nyanfi`.
- Run `scripts/check_commands.py`, `scripts/check_literals.py`, and `git diff --check`.
- For issue branches, commit only task files, push, create a PR, and wait for both CI jobs; do not merge unless explicitly requested.