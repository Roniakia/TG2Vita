# Hardware startup crash — 2026-10-07

User-reported error: C2-12828-1. Provided dump:
`psp2core-1791681736-0x000014276d-eboot.bin.psp2dmp`.
This is separate from the earlier LiveArea installation failure.

## Evidence and cause

The mounted Vita's `app/VTGC00001/eboot.bin` exactly matches the pre-fix VPK
by SHA-256. The dump records a data abort (stop reason 0x30004) in the main
VTGC00001 thread. Its code segment loads at 0x8104F000; the linked executable
base is 0x81000000, a relocation delta of 0x4F000.

Crash PC: 0x822E87D2. LR: 0x810764A1, translating to 0x810274A1 in the linked
executable. That is the return address of `_start`'s first memset call, before
newlib initialization, constructors or `main()`.

The old linker-generated `__memset_veneer` at 0x813F9FC0 loads the literal
0x822E87D1 (Thumb target). No relocation entry covers the literal at 0x813F9FC4.
The correctly rebased memset address should be 0x823377D0. The fixed literal
instead jumps into unrelated code and faults. The same old executable contains
228 unrelocated absolute veneers. Vita3K's preferred load base hid the error.

## Correction and verification

`cmake/tdlib.cmake` now supplies `-Wl,--pic-veneer` to native TDLib application
and test links. The platform smoke target also enables it. This changes only
linker-generated jumps; compile-time PIC remains disabled because the installed
Sony ELF converter does not support its GOT relocations.

The rebuilt memset veneer loads a relative offset, adds the current PC and
branches to the resulting Thumb target. The address calculation was checked
at the preferred base, the dumped hardware base and another shifted base.
`scripts/check-link-veneers.py` rejects the preserved crashing executable and
passes the rebuilt one. CMake runs this check on the linked app before packaging.
The VPK also passes ZIP/LiveArea checks and retains the indexed PNG correction.

Retry artifact: `build/vita_tg-0.3.0-startup-fix.vpk`.
The user subsequently reported that the hardware app displays the missing
telegram.conf message. This confirms progress into main/application startup
after the correction; network initialization and account authorization remain
unverified. Passing the static rebasing checks alone was not evidence of login.

## Future dump handling

Keep the exact executable, VELF and VPK before rebuilding when analyzing a
crash. Dumps are gzip-compressed ELF32 cores; their module information provides
actual segment bases. Translate addresses against the relevant segment, not
an assumed preferred base. Useful tools are VitaSDK readelf, objdump and
addr2line; xyzz/vita-parse-core documents the Vita-specific note layout:
https://github.com/xyzz/vita-parse-core

Raw dumps and local decoded memory stay in ignored build/crash-analysis; they
can contain private runtime data. The committed report records only the
necessary module/register evidence. The original user dump was not modified.

## Later confirmation — 2026-10-07

The user reported version 0.3.1 "Works perfectly" after the startup correction
and embedded app credentials. The delivered app is now the user-accepted
hardware baseline; the earlier pending-retry status is superseded. This report
retains the original crash evidence for future regressions.

## Video startup crash — 2026-10-07 (0.5.0 → 0.5.1)

The user reports that photos work but videos crash, and supplied
`psp2core-1791367328-0x00001c24df-eboot.bin.psp2dmp`. The installed
`ux0:app/VTGC00001/eboot.bin` exactly matches the preserved 0.5.0 VPK by
SHA-256. Matching ELF/VELF/SELF/VPK remain in ignored build/releases/0.5.0;
the original dump was copied/decompressed under ignored
build/crash-analysis/video-0.5.0 before editing. No runtime strings or media
contents are included in this report.

Dump evidence: the app loads at 0x8102C000. The stopped thread is
`avPlayer Controller`, reason 0x30003 (prefetch abort), PC 0x82790790 and
LR 0x8278B181. Neither code address belongs to any remaining loaded module;
SceAvPlayer is absent from the module list. Register r4 contains 0x84312C90,
a mapped heap address consistent with the native player context. The main
thread is waiting for vblank, rather than crashing in image decoding.

The 0.5.0 executable/source explains this state. start_video stores the result
of sceAvPlayerInit at viewer offset 0xE8, then uses a signed `blt` branch at
linked address 0x8103C1EE to treat high-bit handles as initialization errors.
stop_player repeats the signed test at 0x8103C0A6, bypassing Stop/Close, but
still unloads the AVPlayer module. Its controller thread then executes freed
code. This is a confirmed code/lifecycle defect, and the dump is consistent
with that path; the dump does not provide a symbolized native-module backtrace.

The SDK's SceAvPlayerHandle typedef is int and its online return description
says negative values are errors. Native handles are opaque pointer-shaped
32-bit values, so the sign is not an ownership test. Vita3K's
[SceAvPlayer implementation](https://github.com/Vita3K/Vita3K/blob/master/vita3k/modules/SceAvPlayer/SceAvPlayer.cpp)
explicitly describes the native result as a pointer while substituting a UID.
This emulator difference can hide the hardware error. The public SDK remains
unchanged; no external player implementation was copied.

0.5.1 uses zero for no player. Init rejects null, -1 and the AVPlayer error-code
namespace; a high-bit native handle remains owned. Data reads, pause/resume
and Stop/Close all use nonzero ownership checks. Audio is joined and GPU work
finishes before Stop/Close, which precedes module unload on normal completion,
source/decode failure, Circle, logout, deletion and shutdown. The photo decoder
and photo controls were not changed.

`./scripts/test-player.sh` compiles the actual MediaViewer with mock Vita APIs.
Using handle 0x84312C90, it reproduces the old live-player module-unload failure
and passes the corrected source. It also covers initialization/module/source
failure, video data, pause/resume, an active audio reader joined before Close,
invalid frames, completion, deletion/logout and destructor teardown. Existing
image decoder and TDLib/layout tests pass. Release build, relative veneer check,
finished-VPK indexed artwork and ZIP/private-file checks pass.

Retry package: build/vita_tg-0.5.1.vpk, with matching artifacts in
build/releases/0.5.1. This corrects the diagnosed lifecycle fault; successful
native video decoding and sound still require a hardware retry. The user's
photo confirmation is partial 0.5.0 validation, not acceptance of video or the
entire build. The accepted 0.3.1 baseline remains preserved.
