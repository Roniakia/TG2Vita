# Chat media (0.5.1)

Photos, videos, video notes and MPEG4 animations now use native TDLib downloads.
Visible message bubbles request only a bounded JPEG/PNG preview. Cross on the
focused bubble, or tapping a bubble, opens fullscreen and requests the full
supported variant. Captions remain complete and wrapped in the conversation.
Circle closes the viewer, cancels an unfinished full download, and preserves
conversation focus/scroll. Start closes native playback before client shutdown.
Cross retries a failed download. Spoiler previews stay hidden until explicitly
opened; self-destructing media is unsupported and never downloaded by this UI.

JPEG/PNG photos use libjpeg/libpng with bounded dimensions and recoverable decode
errors, uploaded to libvita2d/GXM textures. Fullscreen fit preserves aspect ratio;
L/R changes centered zoom from 1x to 3x. This is the application's native viewer;
it does not launch Sony Photos or export private chat media into the gallery.

Video uses Sony's SceAvPlayer module directly, with GPU-mapped CDRAM frame
allocations and GXM YUV420 textures. Audio from the video is drained on a joined
worker thread to a native BGM audio port. Cross pauses/resumes playback. Frame
reuse/teardown waits for GPU completion. Playback completion, decoder startup
failure/timeout and unsupported frame sizes show a readable state; Circle
returns to the chat. Videos download completely before playback; streaming and
seek controls are not yet implemented. Actual codec compatibility depends on
Vita AVPlayer; arbitrary Telegram MP4/HEVC/WebM files are not guaranteed playable.
The native API contract is documented in the adjacent VitaSDK headers and
https://docs.vitasdk.org/group__SceAvPlayerUser.html .

Other attachment formats (including standalone audio, voice messages, stickers,
GIF animations and documents) currently retain placeholders and explicit
unsupported fullscreen status. JPEG/PNG thumbnails are used for supported video
and MPEG4 animations; MPEG4/WebP/TGS thumbnail formats and minithumbnails are
not decoded. Message edits replace attachment metadata; deletion, logout or a
replacement attachment closes the active viewer.

## Resource and cache policy

Client config (`src/telegram/client_config.hpp`) caps preview transfers at 2 MiB,
photo transfers at 8 MiB and video transfers at 64 MiB. Requests use a finite
TDLib download limit, and updates cancel files whose actual/expected/downloaded
size exceeds the cap. Decode validation independently caps encoded images at
8 MiB, either dimension at 2048 and decoded pixels at 2,097,152. Photo selection
chooses a thumbnail around 160 pixels and the largest available variant within
these decode bounds. Video frame dimensions must be no larger than 1920x1088.
Full images are decoded only on selection, not while browsing history.

At most 128 media file states and four decoded preview textures are retained.
The viewer owns one photo or three native video output frame buffers. Texture
cache eviction occurs before drawing, after the preceding GPU work completes.
Theme preview dimensions and texture count live in `src/ui/theme.hpp`.
File updates feed progress and completed paths; automatic failed downloads do
not retry each frame. Explicit selection/Cross retries the full download.
TDLib manages files under the existing device files directory. These are local
private cache files, never VPK inputs. The limits bound individual transfers and
in-memory references; there is no automatic total on-disk cache eviction yet.

## Validation

`./scripts/test-data.sh` exercises pinned-schema photo/video/animation fixtures,
complete captions, bounded variant selection, lazy request deduplication,
progress/completion, failure/explicit retry, transfer caps/cancellation, cached
files, spoiler/secret suppression, message-content replacement and logout.
Variable-height layout tests retain all long caption text with media space.

`./scripts/test-media.sh` compiles the actual image decoder with host GPU-allocation
stubs and Homebrew libpng/jpeg-turbo. It checks JPEG opaque pixels, PNG alpha,
GPU stride padding, corrupt input without process exit, missing files and
oversized dimensions rejected before texture allocation.

The native Release VPK, linked-executable veneer check and finished archive /
indexed LiveArea checks must pass before delivery. Host tests and compilation
do not verify AVPlayer decoding, sound or touch/controller behavior on a Vita.
The user reports 0.5.0 photos work on hardware. Video crashed because a
high-bit native handle was rejected and AVPlayer was unloaded with a live
controller thread. 0.5.1 corrects the ownership/teardown path; see
[crash analysis](CRASH_ANALYSIS.md). Video/sound hardware verification remains
pending, and 0.3.1 remains the accepted baseline. On-device checks: photo/long caption/zoom,
video thumbnail -> video with sound, pause/resume, Circle during download or
playback, unsupported codec, spoilers, media edit/delete, and logout/shutdown.

`./scripts/test-player.sh` exercises the actual fullscreen player with mock native
APIs, including high-bit pointer-shaped handles, failed initialization/source,
pause/resume, joined audio, invalid frames, completion, deletion/logout and
destructor teardown. Never classify AVPlayer handles by their signed value.
