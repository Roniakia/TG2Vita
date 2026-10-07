# Telegram client research for PS Vita

Reviewed 2026-10-06 against official Telegram documentation and the official
TDLib repository. The user confirmed a Telegram client for PlayStation Vita.
Architecture and release scope remain undecided. No TDLib port or login was
attempted during this research. Recheck living documentation before implementation.

## API choice

A user-account client uses the Telegram API, directly or through TDLib. The Bot
API operates bot accounts; the Gateway API delivers verification messages for
other services. Neither supplies a general user-account client. [API overview](https://core.telegram.org/)

MTProto 2.0 combines binary API serialization, authorization/encryption, and
transport. HTTP/HTTPS or WebSocket transport does not turn it into an ordinary
JSON REST API. TDLib's JSON interface is a local library interface. Cloud chats
use client-server encryption; secret chats are a distinct end-to-end encrypted
feature. [MTProto](https://core.telegram.org/mtproto),
[Secret chats](https://core.telegram.org/api/end-to-end)

## Architecture recommendation and tradeoffs

**Recommendation: investigate native TDLib first, with a bounded feasibility
spike.** This is an engineering recommendation, not an approved architecture.
TDLib handles network, encryption, storage and ordered asynchronous updates.
Its documented platform list does not establish PS Vita support.
[TDLib overview](https://core.telegram.org/tdlib)

| Approach | Benefit | Main uncertainty / cost |
| --- | --- | --- |
| Native Vita UI + native TDLib | Mature client engine; session and data stay on the handheld | Porting platform services, dependency ABI, runtime memory and performance |
| Vita UI + TDLib on a companion server | Keeps the complex engine on a conventional host | Requires hosting/pairing; server holds sessions and accesses message plaintext; adds network dependency |
| Native Vita UI + custom MTProto engine | Full control over size and platform adaptation | Must implement crypto checks, schema evolution, session transport, DC handling and synchronization correctly |

Do not commit to a companion server without user agreement on that product
architecture. Do not start a custom cryptographic implementation merely because
curl works on Vita. A native TDLib failure should first be classified as a
specific platform/dependency/resource blocker.

### Native TDLib feasibility work

Official dependencies: C++17 compiler, OpenSSL, zlib, gperf (build time) and
CMake. It offers static C++ and static JSON targets and uses the Boost Software
License. Pin a TDLib commit and matching schema; its API may change even between
minor/patch versions. [Official README](https://github.com/tdlib/td/blob/master/README.md)

Local implications, inferred from the prior SDK inspection:

- Installed GCC 10.3 meets the documented compiler version floor, but the Vita
  SDK's C++ runtime and platform services still need compilation/runtime checks.
- Static OpenSSL/zlib archives exist. This does not prove required APIs,
  threading, crypto entropy or ABI compatibility; inspect actual installed
  versions rather than relying on the package recipes.
- Verify threads, atomics, sockets/polling, clocks, filesystem behavior and a
  cryptographically secure random source against the pinned TDLib sources.
  These are investigation targets, not already established defects.
- Establish a working sample VPK first; the known CMake helper incompatibility
  currently prevents that validation.
- Separate host code generators from ARM target artifacts when cross compiling.
- Build a minimal static integration, then measure VPK size, startup time,
  peak memory, database growth and reconnect behavior on hardware.
- A successful link is insufficient: validate encryption initialization,
  networking, test authorization, persistence and suspend/resume behavior.

## Registration and authentication

Obtain our own `api_id` and `api_hash` through API development tools at
my.telegram.org. Sample application IDs are unsuitable for an end-user release.
No credentials have been created or supplied. Keep development credentials and
session files out of source control and diagnostic logs.
[Application registration](https://core.telegram.org/api/obtaining_api_id)

Prefer QR login for the initial Vita UX as an engineering proposal: display the
login QR on Vita and approve it in an already signed-in Telegram app. The token
expires and must refresh; raw MTProto also has a DC migration/import path.
With TDLib, follow its authorization states instead of independently implementing
that raw flow. [QR login](https://core.telegram.org/api/qr-login)

Phone login cannot promise SMS: the server selects delivery, and some SMS/call
flows are restricted to official mobile apps. Handle the returned code type,
expiry/resend timing and 2FA challenge. Raw 2FA uses SRP. Telegram documents
separate test DC accounts and recommends validating there before production;
test accounts are public/disposable and must hold no private information.
[Authorization](https://core.telegram.org/api/auth)

For a Vita UI, provide readable waiting/error states, phone/email/code entry,
password masking, cancel/retry, and clear connected/authorized status. Restore
the stored session on restart; do not request a new code on every launch.
Treat authorization keys as account access. A database encryption key stored
beside the database provides limited protection if both are copied; choose key
storage and any user unlock flow deliberately.

## TDLib application behavior

Follow `updateAuthorizationState`: initialize via `setTdlibParameters`, supply
requested input, and allow normal requests when ready. Process responses and
updates in receive order; JSON requests use `@extra` for correlation. Maintain
chat/user caches from updates. Use `loadChats`, positions and `getChatHistory`
for paginated lists; short pages do not necessarily mean all data is loaded.
`sendMessage` accepts `inputMessageText`; sending success and file progress arrive
through updates. These are TDLib names, not raw MTProto RPC names.
[Getting started](https://core.telegram.org/tdlib/getting-started)

Proposed app structure: renderer/input on the UI thread, ordered client-event
processing and a bounded queue to the UI model. Store identifiers without
narrowing to 32-bit values; key message records by chat and message IDs. Track
pending, succeeded and failed sends explicitly. Avoid automatic resend paths
that create duplicate messages. Bound memory caches and lazy-load media;
place writable state under an app-specific `ux0:data` directory.

For custom MTProto, synchronization additionally requires persisted seq/pts/qts
state, per-channel state, deduplication, and recovery with
`updates.getDifference` / `updates.getChannelDifference`. Startup and connection
loss require catch-up, not just fetching recent history.
[Update synchronization](https://core.telegram.org/api/updates)

Surface actionable errors and honor server waits such as FLOOD_WAIT and
SLOWMODE_WAIT rather than tight retries. Distinguish invalid input, session loss,
permissions, transient networking and server failures.
[Error handling](https://core.telegram.org/api/errors)

## Text, media and security

Telegram string fields use UTF-8, while message entity lengths/offsets use
UTF-16 code units. Emoji can span surrogate pairs; byte offsets cannot be used
as entity positions. Vita IME buffers are UTF-16, so define tested conversion
boundaries and font fallback. [Text entities](https://core.telegram.org/api/entities)

Media requires upload/download lifecycle handling rather than only external
URL fetches. Start with bounded on-demand images; preserve placeholders for
unsupported content and avoid presenting unsupported actions as successful.
[File transfer](https://core.telegram.org/api/files)

Direct protocol work must follow Telegram's mandatory security checks, including
key-exchange and received-message validation. Use a maintained engine where
possible. Keep logs free of keys, passwords, codes, QR tokens and private
message content. Test clock/entropy behavior and corrupt-state recovery on the
actual target. [Security guidelines](https://core.telegram.org/mtproto/security_guidelines)

## Release requirements that influence design

Telegram requires an app-owned API ID, prominent disclosure of Telegram API
use, correct basic interoperability, and preservation of privacy/read/typing/
self-destruct behavior. Titles containing Telegram require the Unofficial
prefix; the official logo cannot be used. Access to channels requires support
for official sponsored messages. Recheck these terms before public release.
A small technical prototype is not evidence that all release obligations are
met. [API terms](https://core.telegram.org/api/terms)

Keep TDLib's permissive license distinct from official app sources: copying
GPL-licensed client code has separate obligations.
[Registration/source guidance](https://core.telegram.org/api/obtaining_api_id),
[TDLib license](https://github.com/tdlib/td/blob/master/README.md)

## Proposed development sequence

1. Resolve toolchain compatibility and build/run a minimal Vita VPK.
2. Pin TDLib and test native portability with a minimal static engine harness.
3. Validate authorization on test DCs, state persistence and reconnects.
4. Implement chat list, paginated history and text send/receive with Unicode.
5. Validate send outcomes, read/typing states, permissions, edits/deletions,
   logout/session invalidation and app suspend/resume.
6. Add bounded media support and assess release requirements before widening
   scope to channels or distributing publicly.

Calls, secret chats, stories and advanced media are candidates for later work,
not confirmed exclusions or approved requirements. Final feature scope, UI
stack, title/branding, device baseline, credentials provisioning and architecture
still need decisions. This document records research, not a tested client.

## Implementation decision — 2026-10-07

The user selected a native TDLib client; the architecture recommendation above
is now confirmed for this app. The native static engine and phone/email/code/
password UI have been implemented. QR login remains a future proposal. See
[authentication](AUTHENTICATION.md) for the pinned port, supported states and
actual validation; the original research is historical context.
