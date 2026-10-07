Checks for updates automatically in the background when the app opens. A banner announces a newer version; Triangle opens the update page after sign-in. Downloads still go to ux0:download/ for installation with VitaShell.

Improves Vita HTTPS setup with an in-memory CA bundle, independent OpenSSL random seeding, and IPv4/HTTP 1.1. Failed checks now show a curl error code. TLS verification remains enabled. The reported hardware connection failure still needs a real-Vita retest.

Host updater transport and release-parser tests pass, along with the build and package checks. Message sending remains unimplemented.
