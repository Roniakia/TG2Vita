# Inline emoji

Version 0.4.6 bundles 4,009 Twemoji 17.0.1 PNG designs in one offline RGBA
atlas. Unicode sequences are matched against a generated sorted catalog, longest
first: skin tones, regional flags, keycaps and supported ZWJ sequences occupy one
inline slot. VS16 emoji presentation aliases are accepted; VS15 keeps text
presentation. Original UTF-8 message content is retained. Unsupported sequences
use the existing PGF font; Telegram custom animated emoji/stickers are not rendered.

`src/ui/emoji.hpp` supplies shared sequence boundaries and text width measurement.
Wrapping, painting, sender/forward headers and clipped chat previews use the same
boundaries. Images are 22 pixels with a 24 pixel advance at message scale; adjust
settings in `src/ui/theme.hpp`. The 1536 x 1512 atlas loads once after vita2d init
and is freed after GPU completion at shutdown. It requires about 8.9 MiB of pixel
storage plus texture alignment/loader overhead. A failed atlas load shows visible
placeholder squares with unchanged layout.

Artwork: Twemoji, Twitter and contributors / jdecked contributors,
https://github.com/jdecked/twemoji/tree/v17.0.1, CC BY 4.0.
https://creativecommons.org/licenses/by/4.0/
Graphics are resized and packed with transparent gutters; the full license ships
in `licenses/Twemoji.txt`, and attribution appears in Settings/About.

To reproduce, download the v17.0.1 source archive from
https://codeload.github.com/jdecked/twemoji/tar.gz/refs/tags/v17.0.1 and run
`python3 scripts/prepare-emoji.py /path/to/archive.tar.gz` with Pillow installed.
The script checks the pinned archive SHA-256 before generating atlas/catalog/license.
Normal builds use the prepared files and require no artwork downloads or Pillow.

Host regression checks cover common emoji, modifiers, flags, family/rainbow ZWJ,
keycaps, presentation selectors, mixed text width and intact wrapping with complete
text preservation. Real-Vita image appearance and memory behavior remain unverified.
