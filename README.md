# TODO app for Ubuntu KDE

A simple, lightweight TODO list for KDE Plasma 5 (Qt Quick + Kirigami, KDE Frameworks 5).

The majority of actions can be done straight from your keyboard. Check out the shortcuts.

Used daily by me, @anjankow, and now might also be used by you.


> [!NOTE]
> Code written entirely with AI.

![Demo](docs/demo.gif)


## Keyboard shortcuts

The list is also in the app: *Settings* (the button at the right end of the toolbar) → *Keyboard shortcuts*.

"Selected" means the ticked tasks or, when none is ticked, the highlighted task.

| Key | Action |
| --- | --- |
| `Shift+Enter` | Nothing highlighted: start typing a new task.<br/>In the new task input: add task description.<br/>Task highlighted: open its card |
| `Enter` | In the input: create the task. Task highlighted: open its card |
| `Esc` | Leave the input and remove the highlight |
| `Esc` `Esc` | Pressed twice quickly: untick all tasks |
| `Up` / `Down` | Move the highlight |
| `Shift+Up` / `Shift+Down` | Move the highlight and tick the tasks it passes |
| `Space` | Tick or untick the highlighted task |
| `Ctrl+Up` / `Ctrl+Down` | Move the task up / down in the list |
| `Ctrl+PgDown` / `Ctrl+PgUp` | Next / previous tab |
| `Ctrl+A` | Tick all tasks of the current tab |
| `Ctrl+D` | Duplicate the selected tasks |
| `Shift+Delete` | Delete the selected tasks for good |
| `K` | Complete the selected tasks (can be changed) |
| `Del` | Cancel the selected tasks (can be changed) |
| `P` | Postpone the selected tasks (can be changed) |
| `R` | Restore the selected tasks (can be changed) |
| `Ctrl+Q` | Quit |

### Your own shortcuts

Shortcuts for `complete`, `cancel`, `postpone`, `restore` actions can be changed: open *Settings* → *Keyboard shortcuts*.

## Storage

The tasks are saved in the todo file, `todos.json`, together with your labels and keyboard shortcuts: copy or move that file, and they come along.

The location of `todos.json` is defined in `~/.config/kdetodorc`.

## Install

Releases are on the [Releases](https://github.com/anjankow/todo-ubuntu-kde/releases) page.

- `scripts/install-deb.sh [latest | <version>]` – downloads the Debian package and installs it with apt.
- `scripts/run-appimage.sh [latest | <version>]` – downloads the AppImage and starts it; nothing is installed.

## Build

```bash
sudo apt install build-essential cmake ninja-build extra-cmake-modules \
    qtbase5-dev qtdeclarative5-dev qtquickcontrols2-5-dev \
    libkf5coreaddons-dev libkf5i18n-dev libkf5config-dev libkf5dbusaddons-dev \
    qml-module-qtquick2 qml-module-qtquick-window2 qml-module-qtquick-controls2 \
    qml-module-qtquick-templates2 qml-module-qtquick-layouts qml-module-qt-labs-platform \
    qml-module-org-kde-kirigami2 qml-module-org-kde-qqc2desktopstyle
```

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build
```

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ctest --test-dir build --output-on-failure
```

```bash
sudo cmake --install build
```

Run it with `kdetodo` (or `./build/bin/kdetodo` without installing).
