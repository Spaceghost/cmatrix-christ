#!/usr/bin/env python3
"""Install the optional per-user Wayland integration after building cmatrix."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--binary', type=Path, help='Built cmatrix executable')
args = parser.parse_args()
source = Path(__file__).resolve().parent
binary = args.binary or source.parents[1] / 'build/cmatrix'
if not binary.is_file() or not os.access(binary, os.X_OK):
    parser.error('Build first with make -f Makefile.christ, or pass --binary PATH')
home = Path.home()
for directory in ['.local/bin', '.local/lib', '.local/share/applications', '.config/swaylock']:
    (home / directory).mkdir(parents=True, exist_ok=True)
shutil.copy2(binary, home / '.local/bin/cmatrix')
for script in (source / 'bin').iterdir():
    if not script.is_file() or not script.name.startswith('sharkdeck-'):
        continue
    target = home / '.local/bin' / script.name
    shutil.copy2(script, target)
    target.chmod(0o755)
shutil.copy2(source / 'lib/sharkdeck_lock_theme.py', home / '.local/lib/sharkdeck_lock_theme.py')
config = home / '.config/swaylock/config'
if not config.exists():
    shutil.copy2(source / 'swaylock.conf', config)
for template in (source / 'applications').glob('*.desktop'):
    # Desktop Entry Exec quoting is distinct from shell quoting.
    escaped_home = str(home).replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$')
    content = template.read_text()
    lines = []
    for line in content.splitlines():
        if line.startswith('Exec='):
            command = line[5:].split(' ', 1)
            line = 'Exec="' + command[0].replace('@HOME@', escaped_home) + '"'
            if len(command) == 2:
                line += ' ' + command[1]
        lines.append(line)
    (home / '.local/share/applications' / template.name).write_text('\n'.join(lines) + '\n')
subprocess.run([str(home / '.local/bin/sharkdeck-lock-theme'), '--sync'], check=True)
print('Installed. Review the Sway configuration example in README.md before enabling idle locking.')
