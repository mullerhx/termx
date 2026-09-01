# termx
=======

A dockable, multi-pane terminal emulator for Linux built on wxWidgets and
VTE, with a built-in, encrypted SSH/SFTP/FTP connection manager.

Shamelessly vibe coded with AI agents, by someone with 0% prior knowledge of
wxWidgets.

## Features

- VTE-based terminal panes (full xterm-256color-compatible emulation),
  arrangeable via split panes and dockable layout
- SSH/SFTP/FTP connection tree, organized into color-coded groups
- Connections are stored in a password-protected, encrypted vault
  (AES-256-GCM, PBKDF2-HMAC-SHA256 key derivation) — you're prompted for the
  vault password before the main window appears, and the file is never
  written to disk unencrypted
- Terminal panes close automatically when their underlying process exits
- Configurable terminal appearance (font, size, colors, opacity) and toolbar,
  with preferences persisted between sessions
- Copy/paste via Ctrl+Shift+C / Ctrl+Shift+V

## Requirements

termx currently targets **Linux with GTK3** only — `TerminalPanel` embeds
VTE's native GTK widget directly, so it will not build against wxGTK's other
backends or other platforms.

| Dependency  | Minimum version | Notes |
|---|---|---|
| CMake       | 3.16 | |
| C++ compiler | C++17 support (GCC or Clang) | |
| pkg-config  | any recent | used to locate VTE and Nettle/Hogweed |
| wxWidgets   | 3.0, components `core base propgrid aui` | built against wxGTK3; developed against 3.2.8 |
| VTE         | `vte-2.91` | |
| Nettle + Hogweed | any recent | used for AES-256-GCM and PBKDF2-HMAC-SHA256 (connection vault encryption); developed against 3.10.1 |

## Getting the source

```
git clone <this-repository-url>
cd termx
```

## Building

Out-of-source build (recommended):

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The resulting binary is at `build/termx`.

## Installing

```
cmake --install build --prefix /usr/local
```

(omit `--prefix` to use CMake's default install prefix). This installs the
`termx` binary to `<prefix>/bin`, along with a desktop entry and icon
(`<prefix>/share/applications/termx.desktop`,
`<prefix>/share/icons/hicolor/128x128/apps/termx.png`) so termx shows up in
your desktop's application menu — no separate packaging step needed, as long
as `<prefix>/share` is one of your `XDG_DATA_DIRS` (true by default for both
`/usr` and `/usr/local`). Installing to a system prefix such as `/usr/local`
typically requires `sudo`.

## Per-distro instructions

### Debian / Ubuntu and derivatives

```
sudo apt update
sudo apt install build-essential cmake pkg-config \
    libwxgtk3.2-dev libvte-2.91-dev nettle-dev
```

wxWidgets' package name is version-suffixed and varies by release — on older
releases (e.g. Ubuntu 22.04) it's `libwxgtk3.0-gtk3-dev` instead. If
`libwxgtk3.2-dev` isn't found, run `apt search libwxgtk` to find the right
package for your release.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build --prefix /usr
```

### RHEL / Fedora / Rocky Linux / AlmaLinux and derivatives

```
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake pkgconf-pkg-config wxGTK3-devel nettle-devel \
    "pkgconfig(vte-2.91)"
```

On RHEL, Rocky Linux, and AlmaLinux, `wxGTK3-devel` and the VTE dev package
require [EPEL](https://docs.fedoraproject.org/en-US/epel/) to be enabled
first.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build --prefix /usr
```

### Arch Linux / Manjaro and derivatives

```
sudo pacman -S base-devel cmake pkgconf wxwidgets-gtk3 vte3 nettle
```

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build --prefix /usr
```

## Configuration files

termx stores its settings and connection vault under
`$XDG_CONFIG_HOME/termx` (falling back to `~/.config/termx` when
`XDG_CONFIG_HOME` is unset):

- `settings.conf` — terminal and toolbar preferences (plaintext)
- `connections.conf` — the encrypted connection vault
