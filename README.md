# todo-ubuntu-kde

A simple todo list for KDE Plasma 5 (Qt 5 Widgets + KDE Frameworks 5).
Todos are kept in a JSON text file, `todos.json`, in a folder of your choice
(your home directory by default; change it with the *Storage folder…* button).

## Using it

- **Add a task** – type into the input above the list and press `Enter`;
  the new task appears on top.
  `Shift+Enter` starts the description on the next line.
- **Task card** – click a task title to see and edit title, description and labels,
  or to complete, postpone or cancel the task.
- **Select** – tick the checkbox of one or more tasks (`Ctrl+A` selects all), then use the toolbar:
  *Complete*, *Cancel task*, *Postpone*, *Restore*, *Delete*, *Label*.
  Deleting several tasks at once asks for confirmation; deletion is permanent.
- **Postpone** – until tomorrow, 3 days, 1 week, 1 month or a custom date and time.
  Postponed tasks return to the list when the app is opened after that time.
- **Tabs** – *Tasks*, *Postponed*, *Completed*, *Cancelled*.
- **Reorder** – drag tasks, or `Ctrl+Up` / `Ctrl+Down`.
- **Labels** – the *Labels…* button defines labels (name + one of 12 colors);
  assign them in the task card or to the selected tasks with *Label*; filter with the label buttons above the list
  (a task is shown if it has any of the selected labels).

## Install

Releases are on the [Releases](https://github.com/anjankow/todo-ubuntu-kde/releases) page
(made with *Actions → Release → Run workflow* on `main`).

- `scripts/install-deb.sh [latest | <version>]` – downloads the Debian package and installs it with apt.
- `scripts/run-appimage.sh [latest | <version>]` – downloads the AppImage and starts it; nothing is installed.
- `scripts/run-latest-build.sh` – runs the binary CI built for the latest merge to `main` (needs `gh`, signed in).

## Build

```bash
sudo apt install build-essential cmake ninja-build extra-cmake-modules qtbase5-dev \
    libkf5coreaddons-dev libkf5i18n-dev libkf5config-dev libkf5widgetsaddons-dev libkf5xmlgui-dev
```

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build
```

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

```bash
sudo cmake --install build
```

Run it with `kdetodo` (or `./build/bin/kdetodo` without installing).
