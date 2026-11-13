# todo-ubuntu-kde

A simple todo list for KDE Plasma 5 (Qt Quick + Kirigami, KDE Frameworks 5).
Todos are kept in a JSON text file, `todos.json`, in a folder of your choice
(your home directory by default; change it with the *Storage folder…* button).

## Using it

- **Add a task** – type into the input above the list and press `Enter`;
  the new task appears on top. `Shift+Enter` starts the description on the next line.
- **Highlight** – one click highlights a task; `Up` / `Down` move the highlight, `Esc` removes it.
- **Task card** – double-click a task (or press `Enter` on the highlighted one) to see and edit
  title, description and labels, or to complete, postpone or cancel the task.
- **Quick complete** – hover a task and click the green checkmark at the end of its line.
- **Select** – tick the checkbox of one or more tasks (`Ctrl+A` ticks all), then use the toolbar:
  *Complete*, *Cancel task*, *Postpone*, *Restore*, *Delete*.
  With nothing ticked, the toolbar acts on the highlighted task.
  Deleting several tasks at once asks for confirmation; deletion is permanent.
- **Postpone** – until tomorrow, 3 days, 1 week, 1 month or a custom date and time.
  Postponed tasks return to the list when the app is opened after that time.
- **Tabs** – *Tasks*, *Postponed*, *Completed*, *Cancelled*.
- **Reorder** – drag a task by its handle, or `Ctrl+Up` / `Ctrl+Down` on the highlighted task.
- **Labels** – the *Labels…* button defines labels (name + one of 12 colors).
  Assign them in the task card, or tick tasks and click a label above the list.
  With nothing ticked, clicking a label filters the lists
  (a task is shown if it has any of the selected labels).

## Keyboard shortcuts

"Selected" means the ticked tasks or, when none is ticked, the highlighted task.

| Key | Action |
| --- | --- |
| `Enter` | In the input: create the task. With an empty input (or outside it): open the card of the highlighted task |
| `Shift+Enter` | In the input: new line (the description follows the title) |
| `Esc` | Leave the input and remove the highlight |
| `Up` / `Down` | Move the highlight |
| `Ctrl+Up` / `Ctrl+Down` | Move the highlighted task up / down in the list |
| `Ctrl+A` | Tick all tasks of the current tab |
| `Ctrl+D` | Duplicate the selected tasks (copies go to the bottom of the Tasks list) |
| `Delete` | Cancel the selected tasks |
| `Enter` in the task card title | Save and close the card |

While the input has the focus and contains text, `Up` / `Down`, `Ctrl+A`, `Ctrl+D` and `Delete`
edit the text instead; press `Esc` first (the text stays as a draft) or finish the task with `Enter`.
`Esc` and `Ctrl+Up` / `Ctrl+Down` always act on the task list. Click the input to type again.

## Install

Releases are on the [Releases](https://github.com/anjankow/todo-ubuntu-kde/releases) page
(made with *Actions → Release → Run workflow* on `main`).

- `scripts/install-deb.sh [latest | <version>]` – downloads the Debian package and installs it with apt.
- `scripts/run-appimage.sh [latest | <version>]` – downloads the AppImage and starts it; nothing is installed.
- `scripts/run-latest-build.sh` – runs the binary CI built for the latest merge to `main` (needs `gh`, signed in).

## Build

```bash
sudo apt install build-essential cmake ninja-build extra-cmake-modules \
    qtbase5-dev qtdeclarative5-dev qtquickcontrols2-5-dev \
    libkf5coreaddons-dev libkf5i18n-dev libkf5config-dev \
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
