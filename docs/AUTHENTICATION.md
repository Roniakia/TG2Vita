# Native Telegram authentication

The user selected a fully native client on 2026-10-06. This implementation uses
TDLib in the Vita process; no companion server holds account sessions.

Current baseline: the user confirmed version 0.3.1 "Works perfectly" on
2026-10-07 following the hardware fixes and embedded-credential build. Older
validation notes below describe earlier checkpoints.

## Credentials and login

Default builds (0.3.1 onward) embed the application's api_id and api_hash into
the executable. The user explicitly requested this install-only distribution
model. End users install the VPK and sign in; they do not need telegram.conf or
their own application registration.

For developers, fill ignored `secrets/telegram.conf` with the application's
credentials from https://my.telegram.org/apps, then run `./scripts/build.sh`.
CMake's VITA_TG_EMBED_APP_CREDENTIALS option defaults to ON. The build validates
the local file and generates a mode-0600 header under ignored build/generated.
No actual credential values appear in tracked code, CMake definitions, compiler
arguments or build messages. The executable embeds them; the config file itself
is not packaged. Embedded builds always use production servers, regardless of
the local use_test_dc flag.

This protects source-control/build-log hygiene, not secrecy of shipped app
credentials. The values can be extracted from the executable; SELF compression
and obfuscation are not a confidentiality boundary. These identify the app and
are separate from each user's phone/code/password and account-session keys,
which are never bundled.

For config-only development, configure with
`-DVITA_TG_EMBED_APP_CREDENTIALS=OFF`. Those builds load
`ux0:data/vita-tg/telegram.conf`; the USB/emulator provisioning helpers described
below remain available for that mode. Test-DC accounts remain disposable;
production embedded builds must not be confused with test-client harnesses.

The UI follows TDLib authorization updates and supports phone number, code,
login email, email code and masked two-step password entry. It distinguishes
server-directed delivery methods, guards resend timeouts and flood waits, and
marks signed-in only when TDLib reports authorizationStateReady. Unsupported
registration/payment/device-verification steps are shown explicitly. QR login
is not implemented in this first authentication milestone.

Cross edits the current field through native text entry or reloads credentials.
Square submits the reviewed value; Circle clears it. Triangle resends a code
when allowed; after sign-in, log out through Settings. Start closes TDLib before tearing down
network services. Test and production sessions use separate directories/keys.

## Port and build

Pinned TDLib source revision: `ports/tdlib/REVISION`. The Vita adaptations are
preserved in `ports/tdlib/vita.patch` with supporting code beside it; `.deps`
contains the ignored dependency checkout, not application source.

```sh
mkdir -p .deps
git clone https://github.com/tdlib/td.git .deps/tdlib
git -C .deps/tdlib checkout "$(cat ports/tdlib/REVISION)"
./scripts/setup-jansson.sh
./scripts/build-tdlib.sh
./scripts/build.sh
```

Host generators run with the Mac compiler first. ARM compilation uses native
Clang against the installed GCC 10.3 Vita sysroot/libstdc++; linking and packaging
use VitaSDK. The wrapper matches Cortex-A9, hard-float, Thumb, emulated TLS and
short-enum ABI. This setup is specific to the inspected macOS/GCC 10.3 SDK;
recheck before using another SDK. The usual project-local Intel host-library
workaround is still needed for SDK linker/packaging tools.

TDLib's default PIC setting is disabled for Vita because its GOT relocations
are not supported by the installed ELF converter. Link-time section garbage
collection reduces unused code. Worker stacks are 512 KiB; the app's configured
heap is 128 MiB. These are initial settings, not measured hardware budgets.

The port uses select and loopback TCP wakeups. It supplies a partial-write-aware
scatter write, preserves failure for unsupported memory maps/signals/descriptor
duplication, and adapts filesystem timestamp precision. Session locking is
process-local and supports one foreground game instance with one TDLib engine;
shared session access from another process is unsupported.

Persistent SQLite caching is disabled. The SQLite port registers no file VFS,
so attempts to open a persistent database fail rather than pretend to provide
safe filesystem locking. Authorization uses the TDLib binlog. Implement a
validated Vita VFS before enabling file/chat/message databases.

Session binlogs are encrypted using a generated system-random key. The key is
stored nearby in app data; protect both when making backups. This does not
protect against somebody copying the entire device filesystem. Password/code
input buffers are cleared after use, and TDLib logging is disabled. No login
code or password is written to the application configuration.

## Validation

Credential parsing tests cover valid input, placeholders, missing/duplicate
fields, malformed hashes, integer overflow and unknown settings. The native
port smoke VPK checks system RNG/OpenSSL, TDLib threads/TLS, event wakeups and
file locking/I/O. Full runtime and account-login results are recorded here when
verified; a successful compile alone is not authorization validation.

### Verified on 2026-10-07

- Pinned TDLib static JSON engine and the application VPK build successfully.
- Credential parsing tests pass on the development host.
- All four platform checks pass in Vita3K: system RNG/OpenSSL, worker thread
  and thread-local isolation, loopback event wakeup, file I/O and duplicate
  process-local lock rejection.
- The isolated full-engine harness reaches authorizationStateWaitPhoneNumber,
  closes, reopens the encrypted binlog with the existing key, and closes again.
  It uses fictitious app credentials on test DCs in a separate data directory;
  no phone, verification code or account-login request is submitted.
- The application credentials-required screen renders around 60 FPS.

The static link must retain pthread_cancel and pthread_once: GCC's emulated TLS
uses weak pthread references, and otherwise silently shares thread-local state
between workers or calls a missing function. `cmake/tdlib.cmake` preserves both.

Vita3K currently stubs sceIoSyncByFd. The binlog port closes the drained temporary
file before replacement and reopens at the saved offset while retaining the
original process-local lock. This makes buffered emulator writes visible to
TDLib's existing file-size integrity check. Emulator success does not validate
power-loss durability; hardware sync behavior still needs testing.

Not yet verified: server-side app credentials, code delivery, masked IME entry,
2FA, authorizationStateReady, authenticated session restoration, logout, network
reconnect, suspend/resume, or real hardware memory/performance. No account has
been signed in. Persistent SQLite caches and chat history remain disabled.

To reproduce the emulator harnesses after building the app:

```sh
export VITA_HOST_LIBRARY_PATH="$PWD/.host-libs"
cmake -S tests/vita-port -B build/port-smoke -DCMAKE_BUILD_TYPE=Release
cmake --build build/port-smoke --parallel 2
cmake -S tests/vita-auth -B build/auth-smoke -DCMAKE_BUILD_TYPE=Release
cmake --build build/auth-smoke --parallel 2
```

Install each generated VPK through Vita3K. Title IDs are VTGC00002 (platform)
and VTGC00003 (engine initialization). The latter writes only isolated test data
under `ux0:data/vita-tg-auth-tests`. Diagnostic callbacks in that harness are
limited to fatal errors; the normal app disables TDLib logs.

## Login form update — 2026-10-07

Version 0.3.0 displays Phone / Code / 2FA progress, a reviewable current-step
field, explicit submission, empty/phone validation, delivery guidance and a
resend countdown. Code and password drafts are cleared after submission or a
stage change. Both the review field and native IME mask the 2FA password; the
server-provided password hint is displayed. Registration and other unsupported
states stay explicit.

Only authorizationStateReady produces the green login-success card. A separate
getMe request supplies the account display name; profile requests do not block
code/password submission or logout. Closing/logging out clears displayed account
information. The success card is never shown simply because a code was sent.

The user supplied app credentials and explicitly chose production servers.
`secrets/telegram.conf` now selects `use_test_dc=false` and was installed locally
in Vita3K. Values were not printed or packaged. A real account login still
requires the user to enter their phone, delivered code and 2FA password.

Host tests cover login-form input validation, hidden password rendering and
clearing drafts when authorization advances. The isolated Vita harness also
checks synthetic phone/code/2FA/Ready/profile transitions and invalid code/
password responses. Synthetic Ready fixtures are state-machine tests, not
proof of a real account login.

The 0.3.0 app was launched in Vita3K with the user's production configuration
and visibly reached the phone-entry form at about 60 FPS, with the generated
icon in both the app header and emulator library. Native text-entry interaction
and live account authorization still need user-driven verification; no phone
number or verification code was submitted by the agent. The local emulator
keyboard mapping is X = Cross/edit, Z = Square/submit, C = Circle/clear,
V = Triangle/resend or logout, Enter = Start/exit.

## Provisioning on a real Vita (config-only development mode)

The user reached the configuration-required screen after the startup correction.
Application api_id/api_hash are required by TDLib, but the filled config is
intentionally absent from the VPK. This was the earlier default and is now only required when embedding is OFF.
With embedding ON, end users do not provision a file.

1. Keep the filled `secrets/telegram.conf` on the development computer; Git
   ignores this directory. `config/telegram.conf.example` contains placeholders
   and is safe to track. For the user's real account, use_test_dc is false.
2. In VitaShell, export **ux0** over USB. The mounted volume must contain
   `app/VTGC00001/eboot.bin` and `data/`.
3. Run from the project root (replace the volume name if needed):

   ```sh
   ./scripts/install-config-vita.sh /Volumes/Untitled
   ```

   The helper validates without displaying values, atomically copies only the
   config to `data/vita-tg/telegram.conf`, and refuses to create paths beneath
   a disconnected/wrong mount. It keeps a differing existing config unless
   `--replace` is supplied. It does not change session files or the application.
4. Eject/disconnect USB and relaunch Vita TG. On-device path:
   `ux0:data/vita-tg/telegram.conf`. Alternatively copy the same file to that
   path using VitaShell FTP.

Vita3K uses `./scripts/install-config-emulator.sh`; both wrappers share
`scripts/install-config.py`. Validate the source alone with:
`python3 scripts/install-config.py --source secrets/telegram.conf --check-only`.

The configuration is plaintext on the device. Ignoring it in Git and provisioning
it separately prevents it entering source control or shared VPKs; it does not
make it unreadable to someone with device/storage access. A binary/VPK containing
api_hash can also be extracted, and encrypting it with an embedded key does not
solve that. Host copies are created with mode 0600 where the filesystem supports
Unix permissions; FAT/exFAT/Vita storage permissions are not a confidentiality
boundary. No promise of extraction-proof credentials is made.

Official application credential guidance:
https://core.telegram.org/api/obtaining_api_id

The USB volume was disconnected when this helper was added, so the hardware
copy has not been performed by the agent. Source validation and local fixture
copy/overwrite tests passed without displaying actual credentials.

## Embedded application credentials update — 2026-10-07

Version 0.3.1 supersedes the earlier config-only default at the user's explicit
request. `src/telegram/app_credentials.cpp` supplies generated build credentials
and selects production without consulting any device config. Config-only builds
retain the existing strict file parser. The generator rejects invalid inputs
without displaying values and changes to the local source regenerate the header.

Host tests confirm embedded credentials match the private source and force
production, with no device file. Disabled-generation and invalid-input checks
also pass. The target ELF contains the configured credentials; the VPK contains
the matching built SELF (compressed by the SDK), without a config file, account
session or session key. LiveArea and relative-veneer checks pass. No real account
login has been validated. Latest package: build/vita_tg-0.3.1.vpk.

## User acceptance — 2026-10-07

The user confirmed the delivered build/vita_tg-0.3.1.vpk "Works perfectly."
This establishes a user-accepted real-hardware baseline for the self-contained
app. Keep user-reported acceptance distinct from the agent's host/emulator checks.
Individual phone/code/2FA, logout, session restoration and suspend/resume results
were not itemized in this confirmation; detailed coverage remains to be recorded.

## Account navigation — 0.4.0

Ready now opens Chats rather than a separate login-success card. Account identity
is shown in the sidebar. Contacts and Saved Messages use the same TDLib client;
data request IDs are separate from authentication pending/profile IDs so browser
requests do not block login or logout. Settings offers About Vita TG and Log out.
Leaving Ready clears lists/history and resets the navigation to Sign in.
Host synthetic regression checks run with `./scripts/test-data.sh`; these do not
establish real-account/network/hardware correctness for the new browser.
