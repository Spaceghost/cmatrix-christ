"""Shared palettes and persistent selection for the Matrix lock screen."""
import json
import subprocess
from pathlib import Path

THEMES = {
    'matrix-movie': {
        'name': 'Matrix · Movie', 'movie': True,
        'background': '000000', 'text': 'ddffe9',
        'rain': ['001907', '004718', '00832f', '00bd49', '43ed83', 'ddffe9'],
        'ring': '00bd49', 'highlight': 'ddffe9', 'line': '003b12',
    },
    'purple-blue': {
        'name': 'Purple & Blue',
        'background': '090610', 'text': 'eee5ff',
        'rain': ['281039', '492063', '71339d', 'a65ae8', '7098ff', 'b3dcff'],
        'ring': 'af5fff', 'highlight': '619cff', 'line': '39204f',
    },
    'gruvbox-dark': {
        'name': 'Gruvbox Dark',
        'background': '282828', 'text': 'ebdbb2',
        'rain': ['3c3836', '665c54', '98971a', 'b8bb26', 'fabd2f', 'fbf1c7'],
        'ring': 'd79921', 'highlight': '83a598', 'line': '504945',
    },
}
SELECTION = Path.home() / '.config/sharkdeck/lock-theme.json'
LOCK_CONFIG = Path.home() / '.config/swaylock/matrix.conf'


def selected():
    try:
        key = json.loads(SELECTION.read_text())['theme']
        if key in THEMES:
            return key
    except (OSError, ValueError, KeyError, TypeError):
        pass
    return 'purple-blue'


def sync(key=None):
    key = selected() if key is None else key
    theme = THEMES[key]
    source = (Path.home() / '.config/swaylock/config').read_text()
    colors = {'color': theme['background'], 'ring-color': theme['ring'],
              'line-color': theme['line'], 'key-hl-color': theme['highlight'],
              'ring-clear-color': theme['ring'], 'ring-ver-color': theme['highlight'],
              'ring-caps-lock-color': theme['ring'], 'text-caps-lock-color': theme['text']}
    for suffix in ['', '-clear', '-ver', '-wrong', '-caps-lock']:
        colors['inside' + suffix + '-color'] = theme['background']
        colors['text' + suffix + '-color'] = theme['text']
    colors['separator-color'] = theme['background']
    lines = []
    for line in source.splitlines():
        option = line.partition('=')[0]
        if option in colors:
            line = option + '=' + colors[option] + ('' if option == 'color' else 'ff')
        if option == 'indicator-radius':
            line = 'indicator-radius=23'
        elif option == 'indicator-thickness':
            line = 'indicator-thickness=3'
        elif option == 'font-size':
            line = 'font-size=12'
        lines.append(line)
    # Keep password feedback below the centered cross.
    lines = [line for line in lines if line.partition('=')[0] != 'indicator-y-position']
    # Follow the focused output when previewing or using a larger display.
    height = 320
    try:
        result = subprocess.run(['swaymsg', '-r', '-t', 'get_outputs'],
                                capture_output=True, text=True, check=True, timeout=2)
        outputs = [o for o in json.loads(result.stdout) if o.get('active')]
        output = next((o for o in outputs if o.get('focused')), outputs[0])
        height = output['rect']['height']
    except (OSError, ValueError, KeyError, IndexError, subprocess.SubprocessError):
        pass
    lines.append('indicator-y-position=' + str(max(40, height - 36)))
    temporary = LOCK_CONFIG.with_suffix('.tmp')
    temporary.write_text('\n'.join(lines) + '\n')
    temporary.replace(LOCK_CONFIG)
    SELECTION.parent.mkdir(parents=True, exist_ok=True)
    temporary = SELECTION.with_suffix('.tmp')
    temporary.write_text(json.dumps({'theme': key}) + '\n')
    temporary.replace(SELECTION)
