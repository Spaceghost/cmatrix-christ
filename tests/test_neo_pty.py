"""Exercise exported frames and the real interactive terminal lifecycle."""
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time
import xml.etree.ElementTree as ET

binary = str(Path(sys.argv[1]).resolve())
environment = {**os.environ, 'TERM': 'xterm-256color', 'COLORTERM': 'truecolor', 'LC_ALL': 'C.UTF-8'}
environment.pop('NO_COLOR', None)


def command(*args, **kw):
    return subprocess.run([binary, *args], env=environment, capture_output=True, timeout=15, **kw)


assert command('--help').returncode == 0
assert command().returncode == 1, 'non-TTY animation must fail with an explanation'
assert command('--first-rendition').returncode == 1, 'non-TTY original must not emit control codes or hang'
for args in [('--fps', 'nan'), ('--fps', '1.5'), ('--period', '0'), ('--at', 'inf'),
             ('--size', '-1x20'), ('--size', '400x99999999'), ('--theme', 'missing'),
             ('--export',), ('--unknown',)]:
    assert command(*args).returncode == 1, args
with tempfile.TemporaryDirectory(prefix='neo-christ-') as directory:
    for size in ['8x4', '80x24', '160x54']:
        path = Path(directory) / (size + '.svg')
        assert command('--export', str(path), '--size', size, '--at', '4.5').returncode == 0
        root = ET.parse(path).getroot()
        cols, rows = map(int, size.split('x'))
        assert float(root.attrib['width']) == cols * 8
        assert float(root.attrib['height']) == rows * 16
    assert command('--export', directory).returncode == 1, 'write failures must be reported'


class Session:
    def __init__(self, args=(), rows=24, cols=80, env=None):
        self.master, self.slave = pty.openpty()
        self.resize(rows, cols, notify=False)
        self.original = termios.tcgetattr(self.slave)
        self.process = subprocess.Popen([binary, *args], stdin=self.slave, stdout=self.slave,
                                        stderr=self.slave, env=env or environment, start_new_session=True)
        self.output = bytearray()

    def resize(self, rows, cols, notify=True):
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        if notify:
            self.process.send_signal(signal.SIGWINCH)

    def read(self, duration=0.15):
        chunk = bytearray()
        end = time.monotonic() + duration
        while time.monotonic() < end:
            if select.select([self.master], [], [], min(0.02, max(0, end-time.monotonic())))[0]:
                chunk.extend(os.read(self.master, 262144))
        self.output.extend(chunk)
        return bytes(chunk)

    def send(self, text):
        os.write(self.master, text)

    def finish(self):
        end = time.monotonic() + 8
        while self.process.poll() is None and time.monotonic() < end:
            self.read(0.05)
        assert self.process.poll() is not None, 'process failed to exit (possibly starved keyboard input)'
        self.read(0.05)
        assert self.process.returncode == 0, bytes(self.output[-2000:])
        assert termios.tcgetattr(self.slave) == self.original, 'terminal input settings were not restored'
        assert b'\x1b[?25h' in self.output and b'\x1b[?1049l' in self.output, 'screen/cursor were not restored'

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=3)
        os.close(self.master)
        os.close(self.slave)


session = Session()
try:
    session.read(0.4)
    assert b'\x1b[38;2;' in session.output, 'true color missing'
    assert '▀'.encode() in session.output, 'subcell rendering missing'
    session.send(b' ')
    session.read(0.2)
    assert not session.read(0.2), 'paused animation should remain still'
    session.send(b'trh')
    assert session.read(0.25), 'controls should redraw a paused scene'
    session.resize(1, 1)
    assert session.read(0.2), 'minimized terminal should redraw safely'
    session.resize(32, 110)
    assert session.read(0.25), 'resized terminal should redraw'
    session.send(b' +--q')
    session.finish()
finally:
    session.close()

# High load reproduces render times exceeding the requested frame budget.
session = Session(('--fps', '120'), rows=100, cols=300)
try:
    session.read(0.3)
    session.send(b'q')
    session.finish()
finally:
    session.close()

for sig in [signal.SIGINT, signal.SIGTERM, signal.SIGHUP]:
    session = Session()
    try:
        session.read(0.15)
        session.process.send_signal(sig)
        session.finish()
    finally:
        session.close()

session = Session()
try:
    session.read(0.15)
    session.process.send_signal(signal.SIGTSTP)
    session.read(0.2)
    assert termios.tcgetattr(session.slave) == session.original, 'suspension must restore terminal settings'
    session.process.send_signal(signal.SIGCONT)
    assert session.read(0.25), 'resume must redraw'
    session.send(b'q')
    session.finish()
finally:
    # A failed suspend assertion must not strand a stopped test child.
    if session.process.poll() is None:
        session.process.send_signal(signal.SIGCONT)
    session.close()

for args, env, colors in [
    (('--color', '256'), environment, b'\x1b[38;5;'),
    ((), {**environment, 'NO_COLOR': '1'}, None),
    (('--ascii',), environment, b'\x1b[38;2;'),
    ((), {**environment, 'LC_ALL': 'C'}, b'\x1b[38;2;'),
]:
    session = Session((*args, '--frames', '2'), env=env)
    try:
        session.finish()
        if colors:
            assert colors in session.output
        else:
            assert b'\x1b[38;' not in session.output and b'\x1b[48;' not in session.output
        if args != ('--color', '256'):
            assert all(byte < 128 for byte in session.output), 'ASCII fallback leaked Unicode'
    finally:
        session.close()

session = Session(('--first-rendition',))
try:
    session.read(0.4)
    assert b'\x1b[33m' in session.output, 'the first rendition must launch the original gold code cross'
    session.process.send_signal(signal.SIGINT)
    session.finish()
finally:
    session.close()
print('PASS: CLI, SVG, pause/controls, resize, overloaded input, signals, suspend/resume, color/locale fallback and first rendition')
