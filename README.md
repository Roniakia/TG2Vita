# Vita TG

A PlayStation Vita homebrew client project for the Telegram ecosystem.
The app includes native TDLib authentication, a login-aware sidebar, main chat
list, Telegram contacts, Saved Messages history, and Settings with About and
Log out. Version 0.3.1 is the user-accepted hardware baseline; the new 0.5.1
media viewer fixes a hardware video-startup crash. Photos work according to the
user; native video/sound still needs hardware retry. Chat photos and video
thumbnails load on demand; selection opens a fullscreen photo viewer or native
Vita AVPlayer playback. Message sending remains unimplemented.

## Build

Install VitaSDK and target `libvita2d` (including its dependencies), then:

```sh
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
./scripts/setup-jansson.sh
# Prepare and build pinned TDLib as described in docs/AUTHENTICATION.md.
./scripts/build-tdlib.sh
./scripts/build.sh
```

Output: `build/vita_tg.vpk`. The CMake project uses C++17. Development title ID
`VTGC00001` can be overridden with `-DVITA_TITLEID=...` when configuring CMake;
choose a unique ID before distribution.

`cmake/vita-toolchain.cmake` adapts the older SDK CMake policy requirement for
CMake 4, including compiler probes, without changing the installed SDK.
`scripts/vita-cxx.sh` handles macOS host-library search paths. On Apple Silicon,
an Intel SDK requires Rosetta and Intel versions of its host dylib dependencies.
Our inspected GCC depends on zstd; the ARM Homebrew library cannot satisfy it.
A compatible host-only library can be placed in `.host-libs/` (ignored by Git),
or its directory supplied as `VITA_HOST_LIBRARY_PATH`. A native arm64 SDK is
preferable for long-term development. These libraries run on the Mac; they are
not linked into the Vita application.

To reproduce the optional Intel zstd workaround, download the official
[zstd 1.5.7 archive](https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz)
and run:

```sh
./scripts/setup-legacy-host.sh /path/to/zstd-1.5.7.tar.gz
./scripts/build.sh
```

The setup verifies the archive SHA-256 and builds only the Mac host library.

## Run on Vita

Transfer the VPK to a homebrew-enabled Vita and install it with VitaShell.
The app uses the system PGF font and libvita2d rendering at 960 x 544.

- Before login: Cross edits, Square submits, Circle clears, Triangle resends.
- After login: Up / Down selects Chats, Contacts, Saved Messages or Settings.
- Cross focuses a page and opens the selected chat, contact or setting.
- Up / Down scrolls focused lists and full message text; Circle goes back.
- Square loads more main-list chats or older history, or refreshes contacts.
- Settings contains About Vita TG and Log out. After logout, Cross reconnects
  from the sign-in screen.
- In a conversation, Cross or a bubble tap opens attached media fullscreen.
- In fullscreen: Circle returns; L / R zooms photos; Cross pauses/resumes video.
- Start closes the client and exits.

Lists show five rows at a time, with initial avatars and chat previews.
Conversations use rounded bubbles: incoming on the left, outgoing on the right,
with timestamps and older messages above newer ones. Bubbles expand to include
the entire message, including paragraph breaks. Swipe or use Up / Down to read
long messages directly in the conversation.
Swipe the front touchscreen to scroll; tap a row to open it or a media bubble to view it, the sidebar
to switch pages, the conversation heading to go back, or the status bar to load
more. D-pad navigation and Cross/Circle controls remain available. Photo/video bubbles show a preview and complete caption. Other formats show
an unsupported attachment placeholder.
The initial browser bounds memory to 2,000 cached chats/users, 1,000 contacts
and 500 messages in an open history. Main-list chats load 100 at a time;
Square requests more. Archived chats remain future work. See [media support](docs/MEDIA.md) for
formats, limits and native playback validation.

Host state-machine regression tests: `./scripts/test-data.sh` (macOS developer
tools, prepared TDLib/Jansson sources and Homebrew OpenSSL 3). These use mock
transport and synthetic data; they never sign in or send account requests.
Image decode regression tests: `./scripts/test-media.sh` (Homebrew libpng and
jpeg-turbo). Native player lifecycle regression tests: `./scripts/test-player.sh`.

For a developer build, fill ignored `secrets/telegram.conf` with the app-owned
api_id and api_hash. Default builds inject them into the executable from a
private generated header; users need only install the VPK and sign in. No
telegram.conf is needed on the Vita. Credentials stay out of tracked source and
build logs, but are extractable from the shipped executable.

Config-only development builds can set `VITA_TG_EMBED_APP_CREDENTIALS=OFF` and
use the separate USB/emulator file provisioning helpers. See
[authentication setup](docs/AUTHENTICATION.md) for both build modes and validation.

## Project layout

- `src/main.cpp`: startup, UI rendering, input and cleanup.
- `cmake/`: Vita cross-compilation setup.
- `scripts/`: repeatable build and compiler wrapper.
- `assets/`: original speech-bubble icon, gate image and LiveArea XML.
- `docs/`: SDK knowledge, Telegram research and validation status.

The user selected native TDLib; no companion service is used. Next milestones
are verified account authorization, then chat synchronization and messaging.
Background research and remaining feature decisions are in
[Telegram research](docs/TELEGRAM_CLIENT_RESEARCH.md).

## LiveArea artwork

The hardware installer needs compatible palette PNGs. Re-export modified art
with `./scripts/prepare-livearea.sh` (requires ImageMagick). The build checks
that icon0.png and startup.png are 8-bit indexed PNGs with the expected sizes;
true-color exports can cause installation error 0x8010113D even when Vita3K
renders them. Master images stay in `assets/source/`.

## UI customization

Edit `src/ui/theme.hpp` and rebuild to change the palette, bubble width/padding,
text and timestamp sizes, corner radius, spacing, conversation viewport, sidebar
and list geometry, or touch/scroll settings. These are compile-time settings.
Rendering lives in `src/ui/app_view.cpp`; shared wrapping, bubble geometry and
hit testing live in `src/ui/conversation_layout.hpp`. `src/main.cpp` handles
application lifecycle and input rather than painting individual UI elements.

Saved Messages opens as soon as its sidebar entry is selected. Every conversation
loads at least 10 initial messages when available, fetching additional short
TDLib batches automatically. An empty/non-progressing batch ends that process.
The right stick scrolls conversations directly, alongside touch and D-pad.
Its deadzone and speed are configurable in `src/ui/theme.hpp`; initial history
size and request limit are in `src/telegram/client_config.hpp`.

Older history preloads automatically when you are within two screen heights of
the oldest loaded message, using the same viewport for touch, D-pad and right
stick. Adjust `history_prefetch_distance` in `src/ui/theme.hpp`. Requests are
deduplicated and stop at the end of history or the existing 500-message limit.
A failed automatic request waits for a manual Square retry rather than looping.

Message bubbles show the sender's full name and TDLib profile photo, with an
initial avatar when unavailable/loading. Forwarded messages retain a separate
origin label, including hidden-user names and channel/chat author signatures.
Chats and Contacts load automatically on sidebar selection. Version 0.4.5 fixes
TDLib string-encoded int64 chat-order parsing, which previously hid live chats,
and refreshes the list with getChats/getChat after loading. Real-Vita verification
of this fix and avatar rendering remains pending.

Version 0.4.6 adds offline inline emoji images to messages, headers and previews,
including supported skin tones, flags and joined sequences. See [emoji support](docs/EMOJI.md)
for coverage, memory budget and artwork attribution. Emoji graphics: Twemoji by
Twitter and contributors / jdecked contributors, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/),
resized and packed from [v17.0.1](https://github.com/jdecked/twemoji/tree/v17.0.1).

## Updates and releases

Source and VPK builds: [TG2Vita on GitHub](https://github.com/Roniakia/TG2Vita).
Version 0.6.1 checks automatically on startup and shows a banner for newer releases.
Triangle opens the update page after sign-in. You can also open **Settings → App updates**,
check with Cross, then press Cross to download a newer release to `ux0:download/`. Exit and install it with
VitaShell. See [release workflow and update validation](docs/UPDATES.md).
