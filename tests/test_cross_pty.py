"""Exercise the real ncurses program in a PTY at small and large sizes."""
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import sys
import termios
import time

binary = str(Path(sys.argv[1]).resolve())


def capture(rows, columns, cross):
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, columns, 0, 0))
    env = {**os.environ, 'TERM': 'xterm-256color', 'LC_ALL': 'C.UTF-8'}
    env.pop('CMATRIX_CROSS', None)
    if cross:
        env['CMATRIX_CROSS'] = '1'
    process = subprocess.Popen([binary, '-a', '-b', '-c', '-u', '2'],
                               stdin=slave, stdout=slave, stderr=slave,
                               env=env, start_new_session=True)
    os.close(slave)
    output = bytearray()
    deadline = time.monotonic() + 0.5
    try:
        while time.monotonic() < deadline:
            if select.select([master], [], [], 0.05)[0]:
                output.extend(os.read(master, 65536))
        assert process.poll() is None, 'cmatrix exited unexpectedly'
    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGINT)
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        os.close(master)
    assert process.returncode == 0, process.returncode
    text = output.decode('utf-8')
    assert any('\uff66' <= ch <= '\uff9d' for ch in text), 'Japanese glyphs missing'
    assert ('\x1b[33m' in text) == cross, 'Gold code should only appear in cross mode'


for size in [(20, 67), (26, 80), (74, 240)]:
    for enabled in [False, True]:
        capture(*size, enabled)
print('PASS: Japanese rain, opt-in gold cross, clean exit at three terminal sizes')
