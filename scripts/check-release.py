#!/usr/bin/env python3
"""Check release contents without exposing credentials or device data."""
import subprocess
import sys
import zipfile
from pathlib import Path
root = Path(__file__).resolve().parent.parent
subprocess.run([sys.executable, str(root / 'scripts/check-livearea.py'), '--vpk', sys.argv[1]], check=True)
allowed = {'eboot.bin', 'sce_sys/param.sfo', 'sce_sys/icon0.png',
           'sce_sys/livearea/contents/startup.png', 'sce_sys/livearea/contents/template.xml',
           'licenses/Jansson.txt', 'licenses/TDLib.txt', 'licenses/Twemoji.txt',
           'assets/emoji/atlas.png', 'assets/certs/cacert.pem'}
with zipfile.ZipFile(sys.argv[1]) as archive:
    names = [entry.filename for entry in archive.infolist() if not entry.is_dir()]
    if len(names) != len(set(names)) or set(names) != allowed or archive.testzip():
        sys.exit('Release rejected: unexpected, missing, duplicate or corrupt files')
print('Release package contents verified; no configuration/session files')
