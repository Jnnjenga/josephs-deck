# Joseph's Deck

A lightweight Stream Deck-inspired launcher for Windows. Assign your programmable key to launch `JosephsDeck.exe` and get instant access to configurable shortcut buttons.

## Features

- 8 shortcut buttons per profile (2 rows × 4 columns)
- Multiple profiles — switch between pages with dot indicators at the bottom
- Right-click any button to edit it (name, type, target) without touching JSON
- Drag and drop to reorder buttons
- Dark modern Windows 11 style with rounded corners
- Hover and click visual feedback
- Centered floating window, always on top
- Escape key or click-outside to dismiss
- Automatic icon loading from executables
- Single-instance enforcement
- Configurable via `shortcuts.json`

## Running

Double-click `JosephsDeck.exe`. Make sure these files are in the same folder:
- `JosephsDeck.exe`
- `shortcuts.json`
- `libgcc_s_seh-1.dll`
- `libstdc++-6.dll`

The launcher appears centered on screen. Click a button to launch its shortcut, or press Escape / click outside to dismiss it.

## Configuring Shortcuts

### Using the UI

Right-click any button to edit it — change the name, type, and target path directly from the app.

Use the ⚙ button (top-right) to manage profiles: add, rename, or delete profile pages.

Click the dots at the bottom to switch between profiles.

### Editing shortcuts.json manually

The config file uses this format:

```json
{
  "currentProfile": 0,
  "profiles": [
    {
      "name": "Main",
      "shortcuts": [
        { "name": "VS Code",   "type": "application", "target": "C:/Program Files/Microsoft VS Code/Code.exe" },
        { "name": "GitHub",    "type": "website",      "target": "https://github.com" },
        { "name": "Downloads", "type": "folder",       "target": "C:/Users/YourName/Downloads" },
        { "name": "Notes",     "type": "file",         "target": "C:/Users/YourName/notes.txt" },
        { "name": "IP Config", "type": "command",      "target": "ipconfig /all > C:/Temp/ip.txt" },
        { "name": "",          "type": "application",  "target": "" },
        { "name": "",          "type": "application",  "target": "" },
        { "name": "",          "type": "application",  "target": "" }
      ]
    }
  ]
}
```

Supported shortcut types:

| Type          | Action                              |
|---------------|-------------------------------------|
| `application` | Launch an executable                |
| `website`     | Open URL in default browser         |
| `folder`      | Open folder in Windows Explorer     |
| `file`        | Open file with associated app       |
| `command`     | Run a shell command (hidden cmd)    |

Each profile holds exactly 8 shortcuts. Empty slots (blank name and target) show as dark placeholder tiles.

## Building from Source

**Requires:** MSYS2 with UCRT64 toolchain

1. Install [MSYS2](https://www.msys2.org/)
2. In the MSYS2 UCRT64 terminal: `pacman -S mingw-w64-ucrt-x86_64-gcc`
3. Double-click `build.bat` in this folder

The build script expects g++ at `C:\msys64\ucrt64\bin\g++.exe`. Edit `build.bat` if your MSYS2 is installed elsewhere.

**Or with Visual Studio 2022:**
Open `JosephsDeck.sln` → Build → Build Solution (x64 Release)

## Assigning to a Programmable Key

In your keyboard software or HP Command Center, assign your programmable key to launch:
```
C:\path\to\JosephsDeck.exe
```

## Files

```
main.cpp               - Entry point, COM init, message loop
LauncherWindow.h/cpp   - Window, UI rendering, input handling
ShortcutManager.h/cpp  - JSON config loader and profile manager
ActionExecutor.h/cpp   - Shortcut action runner
EditDialog.h/cpp       - Right-click edit dialog
SettingsDialog.h/cpp   - Profile management dialog
Shortcut.h             - Data types
build.bat              - Rebuild script (MSYS2)
JosephsDeck.sln        - Visual Studio solution
JosephsDeck.vcxproj    - Visual Studio project
```
