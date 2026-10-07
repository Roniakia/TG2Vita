Fixes the identified updater socket-opening failure: legacy installed libc returns ENOSYS for curl’s CLOEXEC request. A private copy of curl redirects that request to a Vita compatibility function that validates the socket. The installed SDK and TDLib file handling are unchanged; nonblocking sockets and TLS verification remain enabled.

This beta passes compatibility tests and build/package checks, but needs confirmation on Vita. Stable/Beta selection, startup checks and detailed errors are retained.
