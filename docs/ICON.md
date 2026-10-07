# Vita TG icon

Generated with the built-in imagegen tool on 2026-10-07. Original opaque console
and chat-bubble artwork; no official Telegram or PlayStation mark.

Master: `assets/source/vita-tg-icon.png`. Packaged 128 x 128 PNG:
`assets/icon0.png`, referenced by CMake as `sce_sys/icon0.png` and loaded from
that path by the app header. The master stays in the workspace and is not
packaged. The first export used macOS sips and produced a true-color PNG. After the user
reported hardware installer error 0x8010113D, both installer-facing images were
exported as 8-bit indexed PNGs with ImageMagick. Re-export with
`./scripts/prepare-livearea.sh`; the CMake VPK target now checks their format
and dimensions using `scripts/check-livearea.py`.

Final generation prompt:

> Use case: logo-brand. Asset type: square PlayStation Vita homebrew application icon for Vita TG, an independent Telegram client. Create a polished original icon: a bold white speech bubble subtly shaped like a handheld gaming console, with a small cyan chat accent, on a deep navy to blue background matching a dark blue messaging UI. Centered simple silhouette, generous safe margins, crisp clean geometric illustration, readable at 128 by 128 pixels. Full square artwork, opaque background. No text, no official Telegram paper-plane logo, no PlayStation logo, no trademark symbols, no watermark. Output a square raster image.
