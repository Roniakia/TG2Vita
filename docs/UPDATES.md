# GitHub releases and updates

Source and public VPK releases: https://github.com/Roniakia/TG2Vita

Version 0.6.1 checks once in the background when the app opens. A banner
notifies you when a newer version is available; Triangle opens App updates after
sign-in. Settings → App updates also remains available. Cross checks the latest published
non-prerelease release; another Cross downloads a newer version. The verified
file is saved as `ux0:download/TG2Vita-vX.Y.Z.vpk` when writable, otherwise
`ux0:data/vita-tg/download/TG2Vita-vX.Y.Z.vpk`. The update screen shows
the actual saved path. Exit with Start and install
that file in VitaShell. Installation retains the app's separate data/session
folder. Circle cancels a transfer; exiting joins the updater before network
shutdown. Checking does not require a GitHub account/token on Vita. Startup failures stay
on the update page and do not interrupt sign-in.
The repository and releases must be publicly readable for device updates.

Automatic VPK installation is deferred. VitaSDK's promoter functions accept an
extracted package directory, rather than a VPK. VitaShell additionally extracts
and prepares package metadata. Updating a running app needs a separate installer
and hardware lifecycle validation. This version downloads and verifies only.
References: https://docs.vitasdk.org/group__ScePromoterUtilUser.html and
https://github.com/TheOfficialFloW/VitaShell/blob/master/package_installer.c
The installed promoterutil.h was checked against that reference.

## Release contract

- CMake project version is canonical: strict X.Y.Z; stable tags use vX.Y.Z; beta tags use vX.Y.Z-beta.N.
- Publish a stable GitHub Release with an asset named `vita_tg.vpk`.
- The GitHub asset must supply a lowercase `sha256:` digest and its exact size.
  The updater rejects absent digests, malformed metadata and packages over 64 MiB.
- The initial asset URL must belong to this repo/tag. Redirects permit HTTPS only.
- HTTPS verifies peer certificates and hostnames using the packaged Mozilla CA
  bundle from https://curl.se/ca/cacert.pem (retrieved 2026-10-07).
  Keep the Vita clock correct. Bundle provenance: https://curl.se/docs/caextract.html
- Metadata is capped at 1 MiB, with connection/transfer/stall timeouts. Files stream
  to a `.part`, are SHA-256/size verified, then renamed; failures remove the partial.
- TLS plus a GitHub-supplied digest protects transport/integrity, not a compromised
  repository maintainer account. There is no independent release signing key.

## Publish future builds

New builds of the feature under development must be published as **beta**
versions, with a beta label/tag and GitHub **Pre-release** selected. Publish or
promote a stable release without the beta tag only after the user explicitly
confirms the feature works correctly. Passing local checks is not confirmation.
In App updates, Left/Right selects Stable or Beta. Stable is the default; Beta
includes stable releases and GitHub pre-releases tagged `vX.Y.Z-beta.N` (N starts
at 1). The saved choice is used by manual checks and the next startup check.
Changing the channel clears the old offer and immediately checks the chosen
channel. Stable ranks above betas of the same version; no channel switch causes
a downgrade. Beta checks examine up to 30 recent releases and select the highest
compatible version. The existing HTTPS, size and SHA-256 rules apply to both.

The publication script reads the version suffix from CMake and marks beta
builds as GitHub pre-releases. After user acceptance, clear the suffix and set
`VITA_TG_STABLE_CONFIRMED=yes` when publishing a stable build.

Builds currently depend on the validated local macOS SDK/TDLib wrappers. GitHub
Actions cross-compilation is not configured or claimed reproducible on Linux.
Install GitHub CLI and authenticate with release-upload access, commit the source,
update docs/RELEASE_NOTES.md and versions (including the SFO version), then run:

```sh
./scripts/release.sh
```

This builds locally, checks linker veneers/indexed artwork and the exact archive
allowlist, pushes the source/tag, and uploads VPK plus SHA256SUMS. A publication
failure may leave a pushed tag; fix the cause and create/upload the release for
that existing tag with `gh release create` rather than rerunning the tag step.
Alternatively use GitHub's Releases UI with the same tag and asset names.
Credentials are injected only from ignored local secrets at build time. Source
uploads exclude secrets, generated headers, dependencies, build files and sessions.
The VPK embeds extractable application credentials as expressly selected by the
user; it never contains phone/code/password/session keys.

## Validation

Host tests validate versions, stable-release selection, asset URL/size/digest
requirements and malformed metadata. `scripts/test-updater.sh` also exercises
the actual updater with mocked transport: TLS options, downloads, hash rejection,
cancellation, missing certificates and transport/option errors. Version 0.6.1
loads the CA bundle into memory, seeds OpenSSL from the Vita RNG independently
of login, selects IPv4/HTTP 1.1 and reports curl error codes. The original device
failure is not yet diagnosed or confirmed fixed. Release 0.6.1 builds with the installed SDK;
archive, indexed artwork and relative-veneer checks pass. Hardware HTTPS, GitHub
redirects, cancellation and manual installation still require a real-Vita test.
