#!/bin/sh
# Build and publish from the validated local Vita toolchain.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
command -v gh >/dev/null || { echo 'Install GitHub CLI (gh) and run gh auth login first.' >&2; exit 1; }
version=$(python3 -c 'import re; print(re.search(r"project\(vita_tg VERSION ([0-9.]+)",open("CMakeLists.txt").read()).group(1))')
tag="v$version"
[ -z "$(git status --porcelain)" ] || { echo 'Commit source changes before publishing.' >&2; exit 1; }
./scripts/build.sh
python3 scripts/check-release.py build/vita_tg.vpk
mkdir -p build/releases
cp build/vita_tg.vpk "build/releases/vita_tg.vpk"
(cd build/releases && shasum -a 256 vita_tg.vpk > SHA256SUMS)
git tag "$tag"
git push origin HEAD "$tag"
gh release create "$tag" build/releases/vita_tg.vpk build/releases/SHA256SUMS --repo Roniakia/TG2Vita --title "TG2Vita $version" --notes-file docs/RELEASE_NOTES.md
