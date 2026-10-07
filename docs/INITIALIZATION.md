# Initial client setup

Historical initial-shell record. Current native engine setup and validation are
in [authentication](AUTHENTICATION.md).

Created 2026-10-06 in the `telegram` directory. Git initialized on `main`;
no remote, commit or public release has been created.

The first implementation is a C++17 / libvita2d shell with local preview
navigation. This renderer is an initial scaffold choice, not a commitment to
full Unicode coverage or the final UI architecture. The app has no Telegram
engine or session data yet. Title ID VTGC00001 is a development choice.

Build investigation:

- Project-local toolchain wrapper fixes the installed helper's old CMake policy
  minimum inside compiler probes.
- Installed GCC host binaries are x86_64. Compiler execution revealed an
  unqualified libz dependency plus an Intel zstd dependency; the available
  Homebrew zstd is arm64.
- Docker daemon was unavailable.
- Official zstd 1.5.7 source was downloaded for a local Intel host-library build;
  no installed SDK files are modified. Source build output lives in /private/tmp.
- The compiler wrapper sets the library search environment after macOS protected
  tools launch, avoiding environment stripping in compiler subprocesses.

Final validation:

- `./scripts/build.sh` configured with GNU C++ 10.3.0, compiled C++17, linked
  the ARM executable, converted VELF/SELF and generated `build/vita_tg.vpk`.
- No application compiler warnings were emitted; legacy SDK CMake deprecation
  warnings remain.
- Package ZIP integrity and required entries were checked, along with metadata,
  image dimensions, LiveArea XML parsing and shell script syntax.
- Device installation/rendering/controller behavior remains untested: no Vita
  connection was supplied. This is a packaged offline shell, not a functioning
  Telegram account client.
- `scripts/setup-legacy-host.sh` reproduces the local zstd workaround from a
  checksum-verified official source archive; `.host-libs` is ignored by Git.

## Later hardware finding — 2026-10-07

The original emulator/package checks did not catch Sony installer artwork
requirements. The user reported `0x8010113D` on a real Vita after the 0.3.0 build.
RGB/RGBA icon and startup images were converted to indexed PNGs, and a mandatory
build check was added. Read the hardware packaging correction in
[project knowledge](PROJECT_KNOWLEDGE.md) before relying on the historical
validation above. The corrected package still awaits a confirmed hardware retry.
