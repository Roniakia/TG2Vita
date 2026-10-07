Adds a writable app-data fallback for VPK downloads. The updater first tries ux0:download/; if unavailable, it uses ux0:data/vita-tg/download/. The update screen shows the saved path for installation through VitaShell. If both locations fail, the filesystem errors are displayed and the download can be retried.

Package size/SHA-256 verification and partial-file cleanup are retained. This beta needs real-Vita download confirmation.
