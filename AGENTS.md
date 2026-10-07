# Project context for future agents

Read `docs/PROJECT_KNOWLEDGE.md` and `docs/TELEGRAM_CLIENT_RESEARCH.md` before
implementing or planning this app.

Use https://docs.vitasdk.org/ as the online Vita API reference during development;
see the reference guidance in `docs/PROJECT_KNOWLEDGE.md` and verify APIs against
the installed SDK headers, local sources and samples before implementation.

This folder is the new application workspace. The adjacent `../vita-sdk` folder
contains independent VitaSDK component clones, not the installed SDK. The
installed SDK currently lives at `/usr/local/vitasdk`.

The user confirmed a Telegram client for PlayStation Vita on 2026-10-06.
The user subsequently selected a native TDLib client. Detailed feature scope
remains undecided. Read `docs/AUTHENTICATION.md` before authentication work. Distinguish
confirmed intent from recommendations in the knowledge documents.

Keep application work here. Consult adjacent SDK sources for APIs and examples;
modify SDK components only when the requested work calls for it. Recheck tool
versions, repository status, and build behavior before relying on the dated
snapshot. Update the knowledge document when architecture or setup changes.

The initial app shell uses C++17 and libvita2d. Build with `./scripts/build.sh`;
read `docs/INITIALIZATION.md` for the legacy macOS SDK workaround and validation.
The current UI integrates native TDLib authentication and a bounded account
browser with photo/video previews and native fullscreen viewing (0.5.1).
Read `docs/MEDIA.md` before media changes; preserve transfer/decode limits, hidden
spoiler previews, unsupported self-destructing media and GPU/player teardown.
AVPlayer handles are opaque high-bit pointers on hardware; never use signed
negative/positive tests for player ownership. Stop/Close before module unload;
keep `scripts/test-player.sh` and the video crash report in `docs/CRASH_ANALYSIS.md`.
Message sending remains unimplemented. Only TDLib authorizationStateReady means authenticated.
Keep actual app credential values out of tracked source and logs. Default
release builds embed app-owned credentials as described below. Keep user session
keys and binlogs out of source control and every VPK.
The ignored local credentials file is for the user to fill; never print it.
Use the pinned TDLib patch and project wrappers; do not modify the installed SDK.

## Hardware packaging rule

The user reported Vita installation error `0x8010113D` after the 0.3.0 icon
change. The package contained RGB/RGBA installer artwork; the likely PNG-format
cause was corrected, but successful hardware installation is not yet confirmed.
Keep `assets/icon0.png` (128 x 128) and `assets/startup.png` (280 x 158) as
8-bit indexed, non-interlaced PNGs. Preserve masters in `assets/source/` and
re-export with `./scripts/prepare-livearea.sh` when changing artwork.
The CMake VPK target runs `scripts/check-livearea.py`; do not bypass this check.
Validate the finished archive with
`python3 scripts/check-livearea.py --vpk build/vita_tg.vpk`.
Vita3K rendering/installation does not establish hardware installer compatibility.
See `docs/PROJECT_KNOWLEDGE.md` and `docs/ICON.md` for the finding and provenance.

## Hardware relocation rule

Enable `-Wl,--pic-veneer` for Vita executable links. A hardware dump confirmed
that this large app's fixed-address linker veneers lacked relocation entries
and crashed before main when Sony's loader rebased it. Vita3K's default base
hid the fault. Keep the mandatory `scripts/check-link-veneers.py` check on the
linked ELF; do not confuse relative branch veneers with compile-time PIC/GOT,
which stays disabled for the legacy ELF converter. Preserve matching build
artifacts before changing code after a crash. See `docs/CRASH_ANALYSIS.md`.

## Application credential build rule

The user explicitly chose an install-only public client with app-owned api_id
and api_hash embedded in the executable. This supersedes the earlier config-only
provisioning rule. Default builds use VITA_TG_EMBED_APP_CREDENTIALS=ON and read
secrets/telegram.conf only at build time. A generated header under ignored build/
provides the app credentials; production users do not need telegram.conf.
Do not put actual values in tracked source, compiler arguments, logs or docs.
The VPK contains the credentials inside eboot.bin, so they are extractable;
never promise binary secrecy. Never package phone/code/2FA input or session keys.
Config-only development builds use VITA_TG_EMBED_APP_CREDENTIALS=OFF and retain
the separate device-file provisioning helpers. Read AUTHENTICATION.md for both.

## Working baseline

On 2026-10-07 the user confirmed build/vita_tg-0.3.1.vpk "Works perfectly"
after real-Vita testing. Treat 0.3.1 as the user-accepted hardware baseline and
preserve its indexed artwork, relative linker veneers and default build-injected
application credentials. Earlier pending-retry notes are historical. Individual
login/lifecycle test cases were not itemized; do not overstate verification.
The later 0.4.0 browser adds initial text history but is not yet hardware-accepted.
Message sending remains unimplemented.

## UI customization

Use `src/ui/theme.hpp` for editable UI colors, layout and gesture settings.
Rendering belongs in `src/ui/app_view.cpp`, with shared full-text wrapping and
variable-height bubble geometry in `src/ui/conversation_layout.hpp`. Messages
must display complete wrapped text in the conversation; do not reintroduce
preview-only bubbles or require a separate reader to see the rest of a message.

TDLib int64 JSON values (especially chatPosition.order) are decimal strings;
preserve the shared numeric parser and string-order regression tests. Bubble
sender headers and forwarding origins are separate; do not substitute a
forwarded origin for the actual sender or resolve hidden forwarding identities.

## Beta publication rule

Publish new builds for the feature currently being developed as beta versions
(GitHub pre-releases, with a beta label/tag). Only publish or promote a build to
a stable release without the beta tag after the user explicitly confirms that
the feature works correctly. Successful builds, automated tests and emulator
checks do not substitute for that confirmation. See docs/UPDATES.md.
