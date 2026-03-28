# GameMode
**GameMode** is a daemon/lib combo for Linux that allows games to request a set of optimisations be temporarily applied to the host OS and/or a game process.

GameMode was designed primarily as a stop-gap solution to problems with the Intel and AMD CPU powersave or ondemand governors, but is now host to a range of optimisation features and configurations.

Currently GameMode includes support for optimisations including:
* CPU governor
* I/O priority
* Process niceness
* Kernel scheduler (`SCHED_ISO`)
* Screensaver inhibiting
* GPU performance mode (NVIDIA and AMD), GPU overclocking (NVIDIA)
* CPU core pinning or parking
* Custom scripts

GameMode packages are available for Ubuntu, Debian, Solus, Arch, Gentoo, Fedora, OpenSUSE, Mageia and possibly more.

Issues with GameMode should be reported here in the issues section, and not reported to Feral directly.

---
## Requesting GameMode

For games/launchers which integrate GameMode support, simply running the game will automatically activate GameMode.

For others, you must manually request GameMode when running the game. This can be done by launching the game through `gamemoderun`:
```bash
gamemoderun ./game
```
Or edit the Steam launch options:
```bash
gamemoderun %command%
```

Note: for older versions of GameMode (before 1.3) use this string in place of `gamemoderun`:
```
LD_PRELOAD="$LD_PRELOAD:/usr/\$LIB/libgamemodeauto.so.0"
```
**Please note the backslash here in `\$LIB` is required.**

---
## Configuration

The daemon is configured with a `gamemode.ini` file. [example/gamemode.ini](https://github.com/FeralInteractive/gamemode/blob/master/example/gamemode.ini) is an example of what this file would look like, with explanations for all the variables.

Configuration files are loaded and merged from the following directories, from highest to lowest priority:

1. `$PWD` ("unsafe" - **`[gpu]` settings take no effect in this file**)
2. `$XDG_CONFIG_HOME` or `$HOME/.config/` ("unsafe" - **`[gpu]` settings take no effect in this file**)
3. `/etc/`
4. `/usr/share/gamemode/`

---
## Note for Hybrid GPU users

It's not possible to integrate commands like optirun automatically inside GameMode, since the GameMode request is made once the game has already started. However it is possible to use a hybrid GPU wrapper like optirun by starting the game with `gamemoderun`.

You can do this by setting the environment variable `GAMEMODERUNEXEC` to your wrapper's launch command, so for example `GAMEMODERUNEXEC=optirun`, `GAMEMODERUNEXEC="env DRI_PRIME=1"`, or `GAMEMODERUNEXEC="env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia __VK_LAYER_NV_optimus=NVIDIA_only"`. This environment variable can be set globally (e.g. in /etc/environment), so that the same prefix command does not have to be duplicated everywhere you want to use `gamemoderun`.

GameMode will not be injected to the wrapper.

---
## Development [![Build and test](https://github.com/FeralInteractive/gamemode/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/FeralInteractive/gamemode/actions/workflows/build-and-test.yml)

The design of GameMode has a clear-cut abstraction between the host daemon and library (`gamemoded` and `libgamemode`), and the client loaders (`libgamemodeauto` and `gamemode_client.h`) that allows for safe use without worrying about whether the daemon is installed or running. This design also means that while the host library currently relies on `systemd` for exchanging messages with the daemon, it's entirely possible to implement other internals that still work with the same clients.

See repository subdirectories for information on each component.

### Install Dependencies
GameMode depends on `meson` for building and `systemd` for internal communication. This repo contains a `bootstrap.sh` script to allow for quick install to the user bus, but check `meson_options.txt` for custom settings. These instructions all assume that you
already have a C development environment (gcc or clang, libc-devel, etc) installed.

#### Ubuntu/Debian
Note: Debian 13 and Ubuntu 25.04 (and later) need to install `systemd-dev` in addition to the dependencies below.
```bash
apt update && apt install meson libsystemd-dev pkg-config ninja-build git dbus-user-session libdbus-1-dev libinih-dev build-essential
```

On Debian 12 and Ubuntu 22 (and earlier), you'll need to install `python3` and `python3-venv` packages to install the latest meson version from `pip`.

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install meson
```

Later you can deactivate the virtual environment and remove it.

```bash
deactivate
rm -rf .venv
```

#### Arch
```bash
pacman -S meson systemd git dbus libinih gcc pkgconf
```

#### RHEL 10 and variants
Note: Older versions of RHEL (and variants) cannot build gamemode due to not exposing libdbus-1 to pkg-config.
(also - don't try and play games on RHEL, come on)

You must have [EPEL](https://docs.fedoraproject.org/en-US/epel/getting-started/) enabled to install all dependencies.
```bash
dnf install meson systemd-devel pkg-config git dbus-devel inih-devel
```

#### Fedora
```bash
dnf install meson systemd-devel pkg-config git dbus-devel inih-devel
```

#### OpenSUSE Leap/Tumbleweed
```bash
zypper install meson systemd-devel git dbus-1-devel libgcc_s1 libstdc++-devel libinih-devel
```

#### Gentoo
Gentoo has an ebuild which builds a stable release from sources. It will also pull in all the dependencies so you can work on the source code.
```bash
emerge --ask games-util/gamemode
```
You can also install using the latest sources from git:
```bash
ACCEPT_KEYWORDS="**" emerge --ask ~games-util/gamemode-9999
```

#### Nix
Similar to Gentoo, nixOS already has a package for gamemode, so we can use that to setup an environment:
```bash
nix-shell -p pkgs.gamemode.buildInputs pkgs.gamemode.nativeBuildInputs
```

### Build and Install GameMode
Then clone, build and install a release version of GameMode at 1.8.2:

```bash
git clone https://github.com/FeralInteractive/gamemode.git
cd gamemode
git checkout 1.8.2 # omit to build the master branch
./bootstrap.sh
```
To test GameMode installed and will run correctly:
```bash
gamemoded -t
```

To uninstall:
```bash
systemctl --user stop gamemoded.service
ninja uninstall -C builddir
```

### Pull Requests
Pull requests must match with the coding style found in the `.clang-format` file, please run this before committing:
```
clang-format -i $(find . -name '*.[ch]' -not -path "*subprojects/*")
```

### Maintained by
Feral Interactive

See the [contributors](https://github.com/FeralInteractive/gamemode/graphs/contributors) section for an extended list of contributors.

---
## License

Copyright © 2017-2025 Feral Interactive and the GameMode contributors

GameMode is available under the terms of the BSD 3-Clause License (Revised)

The "inih" library is distributed under the New BSD license
