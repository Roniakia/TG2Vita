# Vita app workspace knowledge

Current architecture: native TDLib authentication and initial account browser with build-injected app-owned
credentials (0.3.1, user-selected install-only distribution); see the 2026-10-07 update
below and [authentication](AUTHENTICATION.md) for build/runtime status.
Latest hardware status (2026-10-07): the user confirmed version 0.3.1
"Works perfectly" after the indexed-PNG, startup-relocation and embedded-credential
corrections. This is user-reported hardware acceptance; see the updates below.

Historical baseline inspection: 2026-10-06. Sources: adjacent repository documentation,
CMake files, package recipes, example source, Git metadata, and installed tools.
This is a navigation and development baseline, not a complete source audit.

## Current state and intended app

- `/Users/au/dev/my/Vita/telegram` was empty except for `.DS_Store` before this
  document was created. No app source, build system, tests, Git repository, or
  existing app architecture was found.
- `/Users/au/dev/my/Vita/vita-sdk` is a collection of **11 independent Git
  repositories**. The containing folder itself is not a unified SDK checkout.
  All 11 component working trees were clean during inspection.
- The user explicitly confirmed a Telegram client for PlayStation Vita on
  2026-10-06. See [Telegram research](TELEGRAM_CLIENT_RESEARCH.md) for official
  documentation findings, architecture options and proposed next steps.
- No Telegram implementation, TDLib integration, login/session handling, or
  application backend was found in the inspected folders.

## Repository map

All component origins are `https://github.com/vitasdk/<name>.git`.
Paths below are relative to `../vita-sdk`; revisions are inspected HEADs.

| Directory | Revision | Purpose / where to look |
| --- | --- | --- |
| artifacts | 8916f25 | Historical dependency mirrors; README dates to 2018. |
| buildscripts | 3c0b9c7 | SDK superbuild: GCC/binutils, sysroot, component pins, host builds, SDK archives and validation. Start at README.md and CMakeLists.txt. |
| docker | 239feec | Published SDK container construction and smoke tests; documented Linux amd64/arm64 images. |
| newlib | 2e428297c | C runtime and Vita platform support; `newlib/libc/sys/vita` is the platform implementation. Includes upstream sources for unrelated platforms. |
| packages | 771ad43 | Target-library recipes in `<library>/VITABUILD`, patches, dependency metadata. |
| samples | fe8fbef | Small working references for packaging, UI, input, network and other hardware APIs. |
| vdpm | c9ba33b | SDK-local package frontend, Pacman product, signed channels and bootstrap scripts. |
| vita-headers | e66ebe90b | `include/psp2` user APIs, `psp2kern` kernel APIs, `psp2common` shared types; `db` NID imports. |
| vita-makepkg | bbd1b18 | Recipe builder and package metadata/transaction tests. |
| vita-toolchain | e8bc8f7 | Vita-specific host conversion/packaging tools and CMake helpers; not the complete compiler SDK. |
| vitasdk-autobuild | c16126c | Python 3.11 standard-library package scheduler, dependency queue, workers and immutable snapshots. Recipes live in packages, not here. |

## Installed development environment

Observed host: macOS arm64. `VITASDK=/usr/local/vitasdk`;
`arm-vita-eabi-gcc` resolves to that SDK's bin directory and reports GCC 10.3.0.
CMake resolves to `/opt/homebrew/bin/cmake` (smoke output identifies 4.4.0).
Docker executable exists at `/usr/local/bin/docker`; daemon availability was
not tested. Installed archives found include curl, ssl, crypto, SDL2, vita2d,
jsoncpp, jansson, zlib and zstd. Archive presence does not verify compatibility.

**Installed SDK and cloned SDK sources differ.** In particular, the installed
CMake toolchain has an old minimum policy version; the cloned helper requires
CMake 3.16. Do not assume local clones describe installed compiler/package
versions or silently copy helpers into the installed SDK.

## App build and packaging

Use `samples/hello_cpp_world/CMakeLists.txt` as the initial build reference:

1. Set `CMAKE_TOOLCHAIN_FILE` to `$VITASDK/share/vita.toolchain.cmake` before
   calling `project()`.
2. Include `${VITASDK}/share/vita.cmake` after `project()`.
3. Build a C/C++ executable and link required target libraries and Sony stubs.
4. Call `vita_create_self`, then `vita_create_vpk`.
5. Choose an app-specific nine-character title ID, display name and version;
   sample IDs are examples, not an assigned identity for this app.
6. Package icon and LiveArea XML/images under `sce_sys` using VPK FILE entries.

Pipeline: ARM ELF -> Vita ELF (.velf) -> fake SELF / eboot.bin -> VPK with
param.sfo and resources. Target triple is `arm-vita-eabi`; cloned toolchain
sets processor `armv7-a` and confines library/include/package searches to the
target environment. Avoid linking Homebrew host libraries into the app.

Once SDK/CMake compatibility is resolved, prefer an out-of-source build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --parallel
```

The policy override is for the observed installed legacy helper, not an
inherent app requirement. A sample configure without it failed under CMake 4.4.
The override bypasses that first compatibility error but did not propagate into
the compiler probe in the observed build, which failed too. This command is
therefore a starting point, not a verified working build invocation. Resolve
the helper/policy propagation or use a compatible CMake version before app work.

Device deployment reference: samples/README.md describes VitaShell FTP
(default example port 1337), VPK upload and manual installation. No device IP,
firmware, homebrew setup, or emulator availability is known.

## VitaSDK online API reference — added 2026-10-07

The user requested [VitaSDK documentation](https://docs.vitasdk.org/) as a
development knowledge-base reference. Consult it when researching Vita APIs
for this app and future Vita application work. The site documents user and
kernel functions exported by Vita modules; its documentation comes from
[vita-headers](https://github.com/vitasdk/vita-headers). The homepage also links
to [VitaSDK samples](https://github.com/vitasdk/samples) for usage examples.

For this user-space app, start with the user APIs (`psp2`). Kernel APIs are a
separate context and should not be assumed usable by an ordinary application.
Use the online reference to discover functions, types and documented behavior,
then check declarations against `/usr/local/vitasdk/arm-vita-eabi/include` and
the relevant adjacent `../vita-sdk/vita-headers` sources and samples. Online
documentation and cloned sources may differ from the installed SDK; verify
availability, linking and hardware behavior before relying on an API. This
reference supplements the existing build wrappers and hardware validation rules.

## Useful app references

| Concern | Local source |
| --- | --- |
| C++ skeleton, link and VPK configuration | samples/hello_cpp_world |
| Minimal diagnostic screen | samples/debugscreen and samples/common |
| HTTP / HTTPS platform APIs | samples/net_http, net_https, net_http_bsd |
| libcurl integration | samples/net_libcurl |
| JSON parsing | samples/json |
| Controller and touchscreen | samples/ctrl and samples/touch |
| Native text entry | samples/ime; uses UTF-16 buffers and common-dialog update loop |
| SDL rendering | samples/sdl2/redrectangle and samples/sdl3/redrectangle |
| App bubble / LiveArea | samples/pretty_livearea |
| Heap sizing | samples/newlib_heapsize_ctrl |
| Thread/runtime primitives and filesystem | vita-headers/include/psp2/kernel and psp2/io; newlib platform source |

The IME example uses a 960 x 544 display and 1024-pixel framebuffer stride.
Treat example allocation sizes and heap settings as examples, not app budgets.

## Networking findings that affect implementation

The libcurl sample demonstrates NET module loading, `sceNetInit`,
`sceNetCtlInit`, HTTP initialization and teardown. Its networking allocation is
4 MiB. Its implementation is a learning reference and needs review before reuse:

- It disables both TLS peer and hostname verification. Do not carry that into
  authentication or message transport.
- It uses unchecked initialization and allocations, loses the original pointer
  on realloc failure, and leaves response/header resources unfreed.
- Its `if (!imageFD)` check does not correctly classify negative Vita I/O errors.
- It stores CURLINFO_RESPONSE_CODE in an int although curl expects a long.

The local curl recipe is 8.17.0-2 and depends on openssl, zlib and zstd. It builds
static libraries, disables IPv6 and the threaded resolver, and sets a CA path
of `vs0:data/external/cert/CA_LIST.cer`. The local openssl recipe points at a
Vita-specific 1.0.2 fork with `no-threads`. These are recipe facts, not verified
installed-library versions. Confirm actual backend, CA format, verification,
threading behavior and endpoint compatibility before choosing app transport.
Other available recipes include curl-mbedtls, mbedtls, libsodium, asio,
websocketpp and wslay; listing a recipe does not prove suitability.

## SDK maintenance boundary

Use the installed SDK to build the app first; rebuilding the entire superbuild
is separate work. Buildscripts describes static target libraries and private
host dependencies, with host system-runtime exceptions. Native full builds
and staged sysroot/host builds are distinct workflows.

vdpm owns the SDK-local package client. Its docs describe signed Ed25519
channel manifests, hash-verified databases, immutable releases and clean-install
bootstrap. Legacy packages.list state is not an in-place migration target.
Do not bootstrap over the installed SDK or upgrade it without establishing
which installation format is present and why the app needs the change.

Licenses vary between components and libraries. Samples declare CC0; the
scheduler declares MIT. Inspect the specific licenses before copying or shipping
other source. Do not infer a single license for the whole directory collection.

## Decisions still needed before app implementation

- Confirm detailed user-account client feature scope; the Telegram client for
  PlayStation Vita is now confirmed, and Bot API is not the client engine.
- Decide direct protocol integration versus a companion service, and investigate
  portability before selecting TDLib or another client library. Neither is
  present or validated here.
- Define initial features, rendering stack, title ID, firmware baseline and
  target device testing setup.
- Establish session storage, credential handling, reconnect behavior, Unicode
  text rendering/input, bounded caches and background networking requirements.

These are future design questions, not approved architecture. Consult current
primary documentation when investigating external protocols and libraries.

## Validation log

- Read component documentation, inspected representative app samples, CMake
  helper sources, selected networking/UI recipes, Git origins/HEAD/status and
  installed compiler/library presence.
- Tried an out-of-tree hello_cpp_world Release build in
  `/private/tmp/vita-knowledge-smoke` without modifying SDK/sample sources.
- Initial configuration failed because installed helper requests CMake policy
  compatibility older than supported by CMake 4.4. Retried with
  `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`; compiler-probe configuration still failed
  at the same legacy policy minimum inside try_compile. No VPK was produced.
  Compiler identification also reported unknown; successful --version output
  alone does not establish that compilation works.
- No app exists yet; no application tests or device execution were possible.


## App initialization update — 2026-10-06

The workspace now has a Git repository on main and a C++17/libvita2d offline
client shell. See README.md for build commands and docs/INITIALIZATION.md for
validation. `./scripts/build.sh` successfully produces `build/vita_tg.vpk`.
The older unsuccessful sample probes above are historical findings, superseded
for the new app by project-local CMake and host-library wrappers. A local Intel
zstd dylib satisfies the legacy compiler dependency on this arm64 Mac. No SDK
files were changed. Device behavior and native TDLib integration remain untested.

## Native authentication update — 2026-10-07

The user selected native TDLib and an ignored local credentials file. This
supersedes the earlier architecture questions and offline shell status. The
app now links pinned TDLib 1.8.67 (revision in `ports/tdlib/REVISION`) and Jansson
2.15.1, and implements authorization-state-driven native IME entry. Read
[authentication](AUTHENTICATION.md) for commands, port limitations and current
runtime validation before changing this integration.

ARM TDLib compilation uses native Mac Clang with the installed GCC 10.3 sysroot
and libstdc++, hard-float/Thumb/short-enum ABI, emulated TLS and no PIC. Host
code generators run separately. SDK linker/packaging tools retain the local
Intel host-library workaround. Installed SDK files were not modified.

Vita3K is installed locally in ignored `.tools/Vita3K.app`, with Sony firmware.
The full app displays the credentials-required screen at approximately 60 FPS.
Platform smoke tests exercise RNG/OpenSSL, threads, sockets/event wakeups and
file I/O/locking. Engine startup testing uses a separate title VTGC00003 and
`ux0:data/vita-tg-auth-tests`, with fictitious credentials and no phone/code
submission. Account authorization remains unverified until real app credentials
and user-driven login are available. Persistent SQLite caches remain disabled;
a validated Vita VFS is still required before enabling them.

## Login form and icon update — 2026-10-07

The user requested a basic login form, 2FA and successful-login confirmation.
Version 0.3.0 adds explicit edit/review/submit controls and a Ready-only success
card with the account name returned by getMe. The user supplied ignored local
app credentials and explicitly selected production. No phone number, code or
2FA password has been supplied in this conversation; account authorization is
still unverified.

An original generated console/chat icon is in `assets/icon0.png` (128 x 128),
with a larger master in `assets/source/vita-tg-icon.png`. It is packaged as the
Vita app icon and used in the header. See `docs/ICON.md` for generation provenance.

## Hardware packaging correction — 2026-10-07

The user reported real-Vita installation error 0x8010113D. Inspection found
PNG color type 2 (RGB) in icon0.png and type 6 (RGBA) in startup.png, matching
PNG-related reports in VitaShell issue #312:
https://github.com/TheOfficialFloW/VitaShell/issues/312

Both installer-facing images are now 8-bit indexed (color type 3), without
interlacing, at 128 x 128 and 280 x 158. Original artwork is retained in
assets/source. scripts/prepare-livearea.sh exports with ImageMagick; the CMake
VPK target runs scripts/check-livearea.py every build. It verifies dimensions,
PNG chunks/checksums, palette and decompressed data. The original incompatible
startup image is rejected by the checker. ZIP and image checks pass on the
rebuilt package; real-hardware installation success awaits the user's retry.
Vita3K rendering alone did not detect this installer-format problem.

The explicitly named retry artifact is
`build/vita_tg-0.3.0-hardware-fix.vpk`; the standard `build/vita_tg.vpk` was also
rebuilt with the corrected images. This is a package-format correction, not a
verified hardware installation or authenticated-account milestone. The same
rule is now in AGENTS.md so future agents encounter it before packaging.

## Hardware startup relocation correction — 2026-10-07

The user subsequently supplied a C2-12828-1 crash dump. Installation progressed
far enough to launch and create an app crash dump; successful app startup remains
unconfirmed. Exact installed-eboot comparison and dump registers identify an
unrelocated absolute linker veneer in `_start`'s memset call, before main.
Real hardware rebased the app by 0x4F000; Vita3K's preferred base concealed it.
Read [crash analysis](CRASH_ANALYSIS.md) for the matching addresses and evidence.

Native application links now require --pic-veneer. Keep compile-time PIC off;
relative branch veneers are a separate linker setting. The new mandatory
scripts/check-link-veneers.py check rejects the crashing ELF and passes the
rebuilt ELF. The latest retry package is build/vita_tg-0.3.0-startup-fix.vpk,
superseding the PNG-only hardware-fix artifact. Hardware startup and actual
account login still await user verification.

## Hardware credential provisioning — 2026-10-07 (historical default)

The user reports that the app now asks for telegram.conf. This establishes that
it progressed beyond the prior before-main crash into application startup; it
does not confirm networking or account authorization. The VPK intentionally
excludes the filled local secrets file. Keep that policy.

Provision `secrets/telegram.conf` separately to ux0:data/vita-tg/telegram.conf,
using scripts/install-config-vita.sh with the mounted ux0 USB volume, or via
VitaShell FTP. The emulator wrapper and hardware wrapper share the validating,
atomic installer scripts/install-config.py. It does not print values; it keeps
a differing existing config unless --replace is specified, rejects wrong or
missing USB mounts and does not alter sessions. See AUTHENTICATION.md for steps.
The volume was disconnected during helper implementation; hardware provisioning
still awaits reconnection. The actual local config validates successfully.

Secrets in native client binaries/packages or readable device files are
extractable. Separate provisioning prevents accidental Git/VPK exposure; it
is not extraction-proof storage. At that stage, config-only provisioning was the default; the later public-client
credential decision below supersedes its prohibition on binary embedding. Actual
values must still stay out of tracked source, compiler arguments and logs.

## Public-client credential decision — 2026-10-07

The user explicitly corrected the earlier config-only approach: a published
native client must include its application api_id/api_hash so end users can
install and sign in without telegram.conf. This supersedes the old prohibition
on embedding app credentials in the executable. It does not authorize committing
actual values to public source or bundling any user's sessions/passwords.

Default CMake option VITA_TG_EMBED_APP_CREDENTIALS=ON validates ignored
secrets/telegram.conf and injects constants through ignored build/generated/
application_credentials.hpp. app_credentials.cpp supplies them without reading
a device file and forces production servers. OFF retains config-only developer
provisioning. Shipped app credentials are extractable from the binary; generated
headers, secrets and values stay out of tracked source/logs/compiler arguments.

0.3.1 builds and credential-source/runtime-selection tests pass, as do both
hardware packaging regression checks. Latest install-only VPK:
build/vita_tg-0.3.1.vpk. Real account authorization remains unverified.

## User-confirmed working baseline — 2026-10-07

After receiving build/vita_tg-0.3.1.vpk, the user reported: "Works perfectly."
Record 0.3.1 as the current user-accepted real-Vita baseline. This supersedes
older notes awaiting a hardware retry, and confirms the delivered self-contained
build works in the user's setup. Confirmation comes from the user, not an
independently observed hardware test by the agent. The user did not enumerate
individual login/2FA/logout/restart/suspend tests; do not invent that coverage.

Preserve the working setup in future changes:

- Native TDLib in the Vita process; no companion service.
- Embedded app-owned credentials by default, injected from ignored local build
  input; no telegram.conf required on the device for the default build.
- Production servers; user codes/passwords/session keys remain runtime-only.
- Indexed LiveArea PNGs and mandatory artwork validation.
- --pic-veneer and mandatory linked-executable relocation checks.
- Explicit login form, masked 2FA and Ready-only account confirmation.

Build with ./scripts/build.sh. Detailed setup is in AUTHENTICATION.md; crash
root cause and regression evidence are in CRASH_ANALYSIS.md. Chat history and
message sending remain unimplemented, regardless of the working auth milestone.

## Account browser update — 2026-10-07

The user requested an authenticated sidebar with Chats, Contacts, Saved Messages
and Settings. Version 0.4.0 hides these while signed out, showing Sign in only.
Chats uses loadChats and TDLib main-list position updates, sorted by 64-bit order;
Contacts uses getContacts/getUser. Selecting a chat/contact opens text history.
Saved Messages uses getMe's user ID and createPrivateChat, then getChatHistory.
Settings contains an About Vita TG page (Telegram API/TDLib disclosure) and logOut.
Only authorizationStateReady exposes authenticated navigation. Leaving Ready
clears cached account data; closed clients reconnect with Cross on Sign in.

History is newest-first, with manual older-message loading and full-text reading.
Text edits, deletions and incoming messages update the open history; media and
service content has placeholders/captions. Request correlation and canceled
history requests prevent late replies from changing a newly opened chat.
Cached sorted list rows avoid sorting every rendered frame. Data stays in RAM;
the disabled SQLite cache and existing binlog/key handling are unchanged.
Bounds: 2,000 cached chats/users, 1,000 contacts, 500 history messages. Chats
loads 100 main-list entries per request; Square loads more. Archived chats,
message sending, media rendering and read receipts are still unimplemented.

0.3.1 remains the user-accepted hardware baseline, preserved as
build/vita_tg-0.3.1-baseline.vpk before this change. The new package is
build/vita_tg-0.4.0.vpk. Host synthetic authentication/browser tests cover
ordering, removal from the main list, contacts, self-chat routing, history
pagination/deduplication, edits/deletions, stale replies and logout cleanup.
The new release builds and passes indexed-artwork/archive and relocation checks.
No new real-account or hardware test was performed by the agent; user validation
of the new views remains outstanding. Artwork, relative linker veneers and
build-injected app credentials retain the accepted baseline rules.

## Vita-style conversation UI — 2026-10-07 (0.4.1)

The user requested message bubbles and a more native Vita feel. Conversations
now show rounded, left-aligned incoming and right-aligned outgoing bubbles with
TDLib message dates formatted as local timestamps. Older messages sit above
newer ones; the initial view sits at the newest messages. A cyan outline marks
the focused bubble, with an on-screen scroll indicator. Cross opens full text;
long preview text is limited to two lines with a read hint. Word wrapping
preserves UTF-8 boundaries. A small bounded layout cache avoids remeasuring
long messages each frame and clears when signed out.

Chat/contact rows use generated initial avatars (not downloaded profile photos).
Chat rows include TDLib last-message previews and track last-message updates.
Front touchscreen swipes move the conversation viewport directly, while D-pad
moves focus between bubbles. Lists and full text also accept swipe navigation.
Taps open rows/bubbles/settings, switch sidebar pages, go back through the page
heading, or load more through the status bar. Touch coordinates use queried
panel bounds. Finger movement is separated from taps. Existing authentication,
indexed artwork, relative veneers and embedded credential policies remain.

Latest package: build/vita_tg-0.4.1.vpk. Host authentication/browser regression
tests include message dates and chat previews; the native VPK, installer artwork,
archive and relocation checks pass. Real-Vita appearance, touch responsiveness
and timezone/font behavior still need user testing. 0.3.1 remains the accepted
hardware baseline. These are initial avatars and text/media placeholders; no
profile-photo or message-media downloads were added.

## Full-text bubbles and UI configuration — 2026-10-07 (0.4.2)

The user requested that messages always show their full text, and that UI colors
and configuration move out of main. The two-line bubble previews and separate
Cross-to-read view are removed. Each bubble wraps the complete TDLib text with
paragraph breaks and sizes itself to all lines. Messages taller than the screen
are read by scrolling the conversation with touch or Up/Down. Rendering clips
only to the screen viewport; neither the model nor bubble layout truncates text.
Variable bubble heights now determine scrolling, hit testing and the scroll
indicator. Existing message layouts are reused on updates, and data clears on
logout. Focus anchors use measured offsets when new messages arrive.

`src/ui/theme.hpp` is the editable compile-time palette/layout configuration:
colors, viewport, bubble width, padding, text scales, line spacing, corner
radius, list/sidebar geometry and gesture/scroll settings. Rendering is in
`src/ui/app_view.cpp`; wrapping and geometry are in
`src/ui/conversation_layout.hpp`. Main retains lifecycle and input orchestration.

Latest package: build/vita_tg-0.4.2.vpk. Host tests cover full 5,000-character
text preservation, paragraph breaks, UTF-8, oversized bubbles, scroll bounds and
hit testing, alongside authentication/browser regressions. Release build and
indexed-artwork/archive/relative-veneer checks pass. The new UI still awaits
user hardware testing; 0.3.1 remains the hardware-accepted baseline.

## Direct Saved Messages, initial history and right stick — 2026-10-07 (0.4.3)

Selecting Saved Messages in the sidebar immediately opens/loads the self chat;
Cross only moves focus into a page already opened. Sidebar touch activation uses
the same page-selection function and avoids a duplicate request. All conversation
opens automatically continue short getChatHistory batches until at least 10
messages are cached, the end is reached, or a request fails. Deduplication and
no-progress termination prevent repeat loops. Threshold and request limit live
in src/telegram/client_config.hpp.

Analog controller sampling is enabled. Right-stick vertical tilt scrolls any
open conversation, including sidebar-focused Saved Messages. Movement uses
elapsed frame time with a pause cap, proportional tilt and configurable deadzone
and maximum speed in src/ui/theme.hpp. Up shows older text, down shows newer.
Existing touch and D-pad paths remain.

Latest package: build/vita_tg-0.4.3.vpk. Synthetic tests cover ten consecutive
short batches, termination at the initial minimum, empty/end-of-history stop,
and analog deadzone/direction/frame-time scaling. The release build and hardware
packaging/relative-veneer checks pass; actual hardware control feel remains for
user validation. The accepted hardware baseline remains 0.3.1.

## Early history prefetch — 2026-10-07 (0.4.4)

The user requested smooth automatic loading near the last available message.
All conversation views prefetch older history when the remaining loaded scroll
extent is at most two viewport heights (theme.hpp history_prefetch_distance).
This runs after input updates and uses measured variable-height bubble geometry,
so touch, D-pad and right stick share the trigger. Short histories automatically
fill the viewport and advance buffer. Older batches append without moving the
existing bubble offsets or current scroll position.

Auth::prefetch_history avoids requests during initial top-up, while another
history request is pending, after end-of-history/500-message limit, or when the
same cursor already had an automatic attempt. An error at a cursor does not
cause per-frame retries; Square remains the explicit recovery path. Leaving a
chat resets the cursor, and canceled requests retain existing correlation rules.

Latest package: build/vita_tg-0.4.4.vpk. Synthetic tests cover the early-loading
boundary, single in-flight request, suppression of automatic error retries and
manual recovery. Existing full-text/history/authentication tests and native
build, archive, indexed-artwork and relative-veneer checks pass. Actual network
latency and hardware scrolling remain for user validation.

## Sender identity, forwarding, avatars and chat list correction — 2026-10-07 (0.4.5)

The user requested sender avatars, full-name bubble titles, forwarded attribution,
a fix for an empty real-Vita chat list, and automatic sidebar data loading.
Inspection of pinned TDLib td/tl/tl_json.h confirmed JsonInt64 serializes as a
JSON string. ChatPosition.order is int64; the app previously used integer-only
parsing, producing zero and hiding chats. The shared numeric reader now accepts
validated decimal strings as well as integers, with overflow rejection.
loadChats continues to maintain positions through updates, with an explicit
getChats snapshot/getChat refresh on success or end-of-list. List errors include
only a numeric code, never raw server text. Selecting Chats/Contacts refreshes
automatically; Ready still initializes both. Saved Messages remains direct.

Messages preserve actual sender identity (user/chat), author signatures, and
forward origins (user, hidden user, chat or channel). Missing identity metadata
is resolved with getUser/getChat. Bubbles have full sender-name headers and a
separate Forwarded from origin header. Hidden forwarding identities stay named
as provided by TDLib, without attempts to resolve a private account. Headers
are wrapped and included in measured height, preserving full message text.

Visible sender profile photos download on demand through native TDLib. Small
user/chat photo files are decoded with Vita JPEG/PNG helpers; initial avatars
remain for absent/loading/unsupported photos. The GPU texture cache is bounded
to 16 entries, evicted before frame drawing after GPU completion, and cleared
on logout/shutdown. Downloads and file references are bounded by client config;
profile photos remain in the normal device files directory, never packaged.
Avatar/title dimensions and texture count are in theme.hpp. Persistent SQLite
remains disabled; no installed SDK or installer artwork changes were made.

Latest package: build/vita_tg-0.4.5.vpk. Host tests exercise real string chat-order
fixtures, explicit list enumeration, user full names, hidden/user/channel
forward attribution, avatar request deduplication and file completion, plus
variable-height headers and all existing history/authentication checks. Release
build and indexed-artwork/archive/relative-veneer checks pass. Sender-photo
appearance, automatic list loading and network behavior need user real-Vita
verification; the parser fault is confirmed in code, not independently tested
with a live signed-in account by the agent.

## Inline image emoji — 2026-10-07 (0.4.6)

Messages now render supported Unicode emoji as bundled Twemoji images inline with
PGF text. Shared longest-sequence matching and width measurement preserve supported
flags, modifiers, keycaps and ZWJ sequences during wrapping and clipping. Sender and
forward headers plus chat previews also use inline rendering. Complete wrapped
message text and bubble geometry are preserved. Editable sizing is in theme.hpp.

A single offline 4,009-image atlas uses approximately 8.9 MiB of RGBA pixel storage;
there is no network fetch or per-message GPU texture cache. Unsupported Unicode
continues through PGF; custom Telegram emoji/animation remains unsupported. The
pinned CC BY 4.0 artwork has packaged licensing and About attribution. See
[emoji](EMOJI.md) for provenance, generation and validation. The 0.3.1 hardware
baseline remains preserved; 0.4.6 emoji appearance is not yet hardware-accepted.

## Saved Messages back navigation — 2026-10-07 (0.4.7)

Circle on Saved Messages now immediately restores sidebar focus while keeping
the conversation, focused message and scroll position. Previously it closed
the history but left content focus active, requiring a second Circle and leaving
the selected Saved Messages page empty. Switching sidebar pages still leaves
the conversation; chat/contact history and About retain their nested back flow.
The shared navigation transition lives in src/ui/navigation.hpp.

Host navigation regression tests cover one-press sidebar return, repeated Back,
preserved Saved Messages history/scroll and nested chat/contact/About returns.
Authentication/browser and full-text layout tests pass. Release build, relative
veneer check, finished-VPK indexed artwork validation and ZIP integrity pass.
Package: build/vita_tg-0.4.7.vpk. Real-Vita verification of this fix remains pending;
0.3.1 remains the user-accepted hardware baseline.

## Chat media previews and native fullscreen viewing — 2026-10-07 (0.5.0)

The user requested chat media previews, selection and fullscreen viewing through
native Vita tools. Photo, video/video-note and MPEG4 animation metadata now comes
from the pinned TDLib schema. Visible bubbles request JPEG/PNG previews lazily;
Cross or touch selection opens a fullscreen viewer with a bounded full download.
Captions stay complete and wrapped. Fullscreen Circle preserves chat navigation
and cancels unfinished full downloads. Spoilers require explicit opening;
self-destructing media and other attachment formats remain unsupported.

Photos use bounded, recoverable libjpeg/libpng decoding and libvita2d/GXM.
Video playback uses Sony SceAvPlayer directly with GPU-mapped frame buffers and
native audio output on a joined worker. This is an in-app native fullscreen
viewer, not a Sony Photos URI handoff or automatic gallery export. See
[media support](MEDIA.md) for formats, configurable limits, cache policy,
validation and remaining playback limitations. Native APIs/headers and the
installed SDK were consulted; no SDK modifications were made.

Synthetic TDLib/media/layout tests and host decoder tests cover download
lifecycle, privacy-sensitive placeholders, captions, corruption and allocation
bounds. Release build, relative linker veneers, finished-VPK indexed artwork
and ZIP validation pass. Package: build/vita_tg-0.5.0.vpk. Native video decode,
sound and media interaction still require real-Vita testing. The accepted
hardware baseline remains 0.3.1; app-owned credential injection and installer
artwork rules remain unchanged.

## Hardware video crash correction — 2026-10-07 (0.5.1)

The user reports 0.5.0 photos work, while videos crash. The supplied dump matches
the installed 0.5.0 executable and records a prefetch abort in `avPlayer
Controller` after its module was unloaded. Inspection found signed checks on
AVPlayer's opaque pointer-shaped handle: a valid high-bit handle was treated as
an error, Stop/Close were skipped, and the module was unloaded with a live
native thread. Read [crash analysis](CRASH_ANALYSIS.md) for addresses/evidence.

0.5.1 changes AVPlayer ownership checks to nonzero handles, rejects null and
known error values at initialization, and preserves audio/GPU/Close-before-unload
ordering. The photo implementation is unchanged. A host test of the actual
MediaViewer using the dump-shaped handle reproduces the old failure and passes
the fix, including failure, pause/resume and shutdown paths. Existing media
image, TDLib and full-text layout tests plus Release/package/veneer checks pass.
Package: build/vita_tg-0.5.1.vpk. The failing 0.5.0 release and private dump are
preserved under ignored build/. Native video/sound awaits the user's retry;
photos have user-reported hardware success. 0.3.1 remains the accepted baseline.

## GitHub distribution and updater — 2026-10-07 (0.6.0)

The user supplied git@github.com:Roniakia/TG2Vita.git for source and VPK releases
and requested an in-app updater, permitting download-to-VitaShell fallback.
Settings → App updates now checks public stable GitHub Releases manually and
streams a newer VPK to ux0:download/. Verified HTTPS uses a packaged Mozilla CA
bundle; strict version/asset URL/size/digest parsing and SHA-256 verification
precede final-file rename. Downloads are capped at 64 MiB; metadata at 1 MiB.
A joined worker handles transfers/cancellation before network teardown.
Installation is manual in VitaShell; no automatic promoter/self-replacement is
implemented. See [updates](UPDATES.md) for API research, release contract,
publication helper, validation and hardware limitations. Version strings in UI,
TDLib and updater now come from CMake's generated version header. Hardware baseline
remains 0.3.1; this release requires hardware updater/network verification.

## Startup update checks — 2026-10-07

Version 0.6.1 checks GitHub Releases once at startup on a worker and shows an
availability banner (Triangle opens App updates after sign-in). The user reported
a generic connection failure in 0.6.0. Transport now loads the packaged CA bundle
into memory, independently seeds OpenSSL via Vita RNG, selects IPv4/HTTP 1.1 and
reports curl codes; certificate verification stays enabled. Host mocked transport
tests cover options, verified downloads, rejection and cancellation. Hardware
resolution remains unconfirmed. A separate tests/vita-updates diagnostic title
writes only public updater results; its emulator runtime has not been verified.

## User-confirmed beta publication policy — 2026-10-07

The user requires new builds of the feature under development to be uploaded as
beta versions until they explicitly confirm that the feature is okay. Mark these
as GitHub pre-releases and use a beta label/tag. Only after that confirmation
create or promote the corresponding stable release without the beta tag. Build,
test or emulator success alone does not authorize stable publication. This rule
applies to future publications; existing releases were not changed by this
documentation update. The current updater selects stable releases only.

## Update channels — 2026-10-07

0.6.2-beta.1 adds a persistent Stable/Beta setting in App updates, changed with
Left/Right. Stable is default. Beta includes stable plus numbered beta pre-releases
and selects the highest valid version among 30 recent GitHub releases. Stable
outranks beta at the same core version; switching never downgrades. Both manual
and startup checks use the saved channel. Beta publication is now the default
in CMake and the release helper; stable publication requires user acceptance.

## Updater connection failure on hardware — 2026-10-07

The user reports curl 7 while Telegram chats load normally in the same session.
The prior transport changes did not establish resolution. 0.6.2-beta.2 adds
CURLINFO_OS_ERRNO and CURLOPT_ERRORBUFFER to surface the actual connection
failure. This is a diagnostic build, not a confirmed fix; keep TLS verification.

## Curl CLOEXEC compatibility — 2026-10-07

The user supplied detailed failure "CLOEXEC: function not implemented". Installed
libc disassembly confirms fcntl always returns ENOSYS, and installed curl calls
F_SETFD/FD_CLOEXEC before TCP connection. 0.6.2-beta.3 uses objcopy on a build-local
curl archive to rename only its fcntl references. The compatibility function
accepts that operation on valid sockets (getsockopt SO_TYPE); other commands
fail. Vita has no exec descriptor inheritance. Curl nonblocking remains native
SO_NONBLOCK; TLS verification is preserved. No installed SDK modification or
global fcntl override. Hardware resolution still needs user confirmation.
