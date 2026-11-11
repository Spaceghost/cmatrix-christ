# cmatrix-christ

A Christian cross in two terminal renditions: the original living-code cross,
and **Lux Crucis**, a slowly rotating, sculpted cross inspired by
[st3w/neo](https://github.com/st3w/neo).

## Lux Crucis — the rotating rendition

![Lux Crucis: a beveled gold cross above quiet Japanese digital rain](docs/neo-christ.png)

[Watch a full 36-second rotation](docs/neo-christ.mp4) (30 fps recording of the
same terminal cells; the live program defaults to a 60 fps limit).

Warm gold, engraved code, a double nimbus and independent Japanese rain streams.
The Latin cross stays upright as it completes a turn every 36 seconds. Its front,
back and beveled sides are actual 3D geometry, rendered in the terminal with
true-color Unicode half blocks and four samples per pixel. Light travels across
the surface as it turns; rotation follows a monotonic clock rather than frame
counts. No GPU, image protocol or downloaded assets are required.

Build and run on Linux or another POSIX system with a C++17 compiler and make:

```sh
make -f Makefile.christ neo
./build/neo-christ
```

Use a UTF-8 locale, a font with half-width Japanese kana, and a terminal with
true-color support. Color support is selected from `COLORTERM` and `TERM`;
`--color truecolor` overrides detection. Smaller terminal text gives more detail.
The default limit is 60 fps; actual throughput depends on the terminal, grid size
and CPU. `--quality low --fps 30` reduces work on small machines.

| Key | Action |
| --- | --- |
| Space | Pause or resume |
| `+` / `-` | Faster / slower rotation and rain |
| `t` | Gold, Matrix green, or ivory palette |
| `r` / `h` | Toggle rain / title and hints |
| `q` / Escape | Exit and restore the terminal |

Ctrl-C, termination signals, window resizing and Ctrl-Z/resume are supported.
Frames update only changed cells; terminals with synchronized-output support
present each frame together. ASCII, 256-color and monochrome fallbacks are available.
`NO_COLOR` selects monochrome ASCII; a non-UTF-8 locale selects ASCII automatically.

```sh
./build/neo-christ --theme ivory --no-rain --quiet
./build/neo-christ --period 48 --color truecolor
./build/neo-christ --ascii --color 256
./build/neo-christ --export cross.svg --size 180x64 --at 4
./build/neo-christ --help
```

SVG export uses the same rendered cells as the live animation and works without a
TTY. `--aspect` adjusts for fonts whose cell width/height differs from 0.5.
The backdrop cache is bounded by the terminal grid; no animation history is kept.
For a smaller device, `--size 80x26 --quality low --fps 30` limits the drawing area.

## First rendition — the original code cross

A fork of [CMatrix](https://github.com/abishekvashok/cmatrix) with a traditional
Christian cross formed from live Japanese code. The cross, gold edges, double
nimbus, flared terminals and moving glints are drawn by ncurses itself. Normal
CMatrix behavior remains available when `CMATRIX_CROSS` is unset.

![Code cross with Matrix rain](docs/cmatrix-christ.png)

### Build both renditions

Install C and C++17 compilers, make, pkg-config and the wide-character ncurses development
package. The optional smoke tests also require Python 3.

```sh
make -f Makefile.christ
./build/neo-christ --first-rendition
# Equivalent original command:
CMATRIX_CROSS=1 ./build/cmatrix -a -b -c -u 2 -C green
make -f Makefile.christ test
```

`-c` selects half-width Japanese kana, `-a` gives streams independent speeds,
`-b` enables bold highlights, and `-u 2` sets the update delay to 20 milliseconds.
Use a UTF-8 locale and a font with Japanese support, such as IPAGothic. This fork
also fixes wide-character rendering of stream heads. The terminal palette controls
colors; the optional Wayland integration supplies the gold and theme palettes.

The code cross scales with terminal geometry. More pixels on a larger output allow
a finer code grid; the built-in Sharkdeck panel remains at its native 480×320.
The capture above was rendered separately at 1440×960.

`make -f Makefile.christ test` exercises both programs in real PTYs, including
pause, resize, overloaded rendering, clean exits, suspend/resume, color and locale
fallbacks. Renderer checks cover deterministic frames, depth and motion continuity.
Build outputs default to `build/`; set `BUILD_DIR` to use another directory.
CMake also builds both programs on POSIX systems; upstream Autotools builds the
original CMatrix program.

## Optional Sway lock screen and theme picker

`contrib/sharkdeck` contains the actual per-user integration used on the deck:

- Matrix Movie, Purple & Blue, and Gruvbox Dark palettes.
- A searchable theme picker and a separate unlocked preview window.
- Real cmatrix in a GTK 3/VTE terminal, embedded through windowtolayer.
- A swaylock-plugin password lock, with standard swaylock as a startup fallback.
- A single-instance idle manager that locks after ten minutes without changing
  display power. Existing Sway idle inhibitors are respected.

Dependencies are Python 3, PyGObject, GTK 3, VTE 2.91 introspection, IPAGothic,
fuzzel, swayidle, swaylock, [swaylock-plugin](https://github.com/mstoeckl/swaylock-plugin)
and [windowtolayer](https://gitlab.freedesktop.org/mstoeckl/windowtolayer).
The locker must have a working `swaylock-plugin` PAM service on your system.
This repository does not install PAM policy or change account passwords.

On Alpine, the packaged portion of these dependencies can be installed with:

```sh
doas apk add build-base pkgconf ncurses-dev python3 py3-gobject3 gtk+3.0 vte3 font-ipa fuzzel swayidle swaylock
```

Install the integration into your own home directory after building and installing
the two external Wayland helpers:

```sh
python3 contrib/sharkdeck/install.py
```

The installer copies user scripts and launcher entries to `~/.local`, generates
the selected theme, and preserves an existing `~/.config/swaylock/config`.
It does not edit or reload Sway configuration. Review and add these bindings to
Sway to enable the features you want:

```sway
bindsym $mod+BackSpace exec ~/.local/bin/sharkdeck-lock
bindsym $mod+Ctrl+t exec ~/.local/bin/sharkdeck-lock-theme
exec_always --no-startup-id ~/.local/bin/sharkdeck-idle
```

Replace an existing lock binding or idle power-off configuration rather than
running duplicate idle managers. Check your account password and locker setup
before enabling automatic locking.

Search your application launcher for **Lock Screen Themes** or **Preview Lock
Animation**. Escape or Q closes the unlocked preview. Theme changes apply to the
next lock or preview. The code cross is part of cmatrix, while password entry is
handled only by the surrounding locker.

## License and upstream

GPL-3.0-or-later, as in upstream CMatrix. Original copyright notices and `COPYING`
are retained. Lux Crucis is an original renderer inspired by Stewart Reive's
[neo](https://github.com/st3w/neo), especially its graded asynchronous rain,
half-width kana and true-color treatment; it does not embed neo's source.
The first rendition is preserved in `cmatrix.c` and remains the default for the
existing Sharkdeck integration. The upstream README follows for the original
build systems and terminal controls.

---

<h1 align="center">CMatrix</h1>

<h3 align="center"> Matrix like effect in your terminal </h3>

</p>
<p align="center">
  <a href="https://travis-ci.org/abishekvashok/cmatrix">
    <img src="https://travis-ci.org/abishekvashok/cmatrix.svg?branch=master">
  </a>
  <a href="./COPYING">
    <img src="https://img.shields.io/github/license/abishekvashok/cmatrix?color=blue">
  </a>
  <img src="https://img.shields.io/badge/contributions-welcome-orange">
  <a href="https://github.com/abishekvashok/cmatrix/stargazers">
    <img src="https://img.shields.io/github/stars/abishekvashok/cmatrix">
  </a>
  <a href="https://github.com/abishekvashok/cmatrix/network">
    <img src="https://img.shields.io/github/forks/abishekvashok/cmatrix">
  </a>
</p>


![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## Contents
- [Overview](#overview)
- [Build Dependencies](#build-dependencies)
- [Building and Installation](#building-and-installing-cmatrix)
    - [Using configure (recommended)](#using-configure-(recommended-for-most-linux%2Fmingw-users))
    - [Using CMake](#using-cmake)
- [Usage](#usage)
- [Captures](#captures)
    - [Screenshots](#screenshots)
    - [Screencasts](#screencasts)
- [Maintainer](#maintainers)
    - [Contributors](#our-contributors)
- [Contribution Guide](#contribution-guide)
- [License](#license)

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :cloud: Overview

CMatrix is based on the screensaver from The Matrix website. It shows text
flying in and out in a terminal like as seen in "The Matrix" movie. It can
scroll lines all at the same rate or asynchronously and at a user-defined
speed.

CMAtrix is inspired from 'The Matrix' movie. If you haven’t seen this movie and you are a fan of computers or sci-fi in general, go see this movie!!!

> :grey_exclamation:`Disclaimer` : We are in no way affiliated in any way with the movie "The Matrix", "Warner Bros" nor
any of its affiliates in any way, just fans.

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :open_file_folder: Build Dependencies
You'll probably need a decent ncurses library to get this to work. On Windows, using mingw-w64-ncurses is recommended (PDCurses will also work, but it does not support colors or bold text).
<br>
##### :small_blue_diamond: For Linux<br>
Run this command to check the version of ncurses.
```
ldconfig -p | grep ncurses
```
If you get no output then you need to install ncurses. Click below to install ncurses in Linux.
- [ncurses](https://www.cyberciti.biz/faq/linux-install-ncurses-library-headers-on-debian-ubuntu-centos-fedora/)

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :floppy_disk: Building and installing cmatrix
To install cmatrix, Clone this repo in your local system and use either of the following methods from within the cmatrix directory.

#### :small_blue_diamond: Using `configure` (recommended for most linux/mingw users)
```sh
autoreconf -i  # skip if using released tarball
./configure
make
make install
```

#### :small_blue_diamond: Using CMake
Here we also show an out-of-source build in the sub directory "build".
(Doesn't work on Windows, for now).
```sh
mkdir -p build
cd build
# to install to "/usr/local"
cmake ..
# OR 
# to install to "/usr"
#cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
make install
```

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :bookmark_tabs: Usage

After you have installed **cmatrix** just type the command `cmatrix` to run it :)
```sh
cmatrix
```
Run with different arguments to get different effects.
```sh
cmatrix [-abBflohnsmVx] [-u update] [-C color]
```
Example:
```sh
cmatrix -ba -u 2 -C red
```

For more options and **help** run `cmatrix -h` <br>OR<br> Read Manual Page by running command `man cmatrix`

_To get the program to look most like the movie, use `cmatrix -lba`_
_To get the program to look most like the Win/Mac screensaver, use `cmatrix -ol`_

> :round_pushpin: _Note: cmatrix is probably not particularly portable or efficient, but it won't hog
**too** much CPU time._

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :camera: Captures

#### :small_blue_diamond: Screenshots

<!-- ![Special Font & bold](data/img/capture_bold_font.png?raw=true "cmatrix -bx") -->
<p align="center">
<img src="./data/img/capture_bold_font.png" alt="cmatrix screenshot">
</p>

#### :small_blue_diamond: Screencasts

<!-- ![Movie-Like Cast](data/img/capture_orig.gif?raw=true "cmatrix -xba") -->
<p align="center">
<img src="./data/img/capture_orig.gif" alt="cmatrix screencast">
</p>

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :zap: Maintainers
- ➤ **Abishek V Ashok** [Core] - <abishekvashok@gmail.com><br> 
<p align="center">
  <a href="https://twitter.com/abishekvashok">
    <img src="https://img.shields.io/badge/Twitter-1DA1F2?style=for-the-badge&logo=twitter&logoColor=white">
  </a>
  <a href="https://github.com/abishekvashok">
    <img src="https://img.shields.io/badge/GitHub-100000?style=for-the-badge&logo=github&logoColor=white">
  </a>
</p>


## :busts_in_silhouette: Our Contributors
#### :small_orange_diamond: Thanks to
- ➤ **Chris Allegretta** <chrisa@asty.org> for writing cmatrix up in a fortnight and giving us
  the responsibility to further improve it.
- ➤ **Krisjon Hanson** and **Bjoern Ganslandt** for helping with bold support and
  Bjoern again for the cursor removal code, helping with the `-u` and `-l`
  modes/flags, and Makefile improvements.
- ➤ **Adam Gurno** for multi-color support.
- ➤ **Garrick West** for debian consolefont dir support.
- ➤ **Nemo** for design thoughts and continuous help and support.
- ➤ **John Donahue** for helping with transparent term support
- ➤ **Ben Esacove** for Redhat 6 compatibility w/matrix.psf.gz
- ➤ **jwz** for the xmatrix module to xscreensaver at http://www.jwz.org/xscreensaver.
- Chris Allegretta's girlfriend **Amy** for not killing him when he stayed up till 3 AM
  writing code.
- ➤ **Sumit Kumar Soni** for beautifying the README.
- The makers of the Matrix for one kickass movie!
- ➤ Everyone who has sent (and who will send) us and Chris mails regarding
  bugs, comments, patches or just a simple hello.
- ➤ Everyone who has contributed to the project by opening issues and PRs on the github repository.

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :book: Contribution Guide
If you have any suggestions/flames/patches to send, please feel free to:
- Open issues and if possible label them, so that it is easy to categorise features, bugs etc.
- If you solved some problems or made some valuable changes, Please open a Pull Request on Github.
- See [contributing.md](./CONTRIBUTING.md) for more details.

![-----------------------------------------------------](https://raw.githubusercontent.com/andreasbm/readme/master/assets/lines/rainbow.png)

## :page_facing_up: License
This software is provided under the GNU GPL v3. [View License](./COPYING)

