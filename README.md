# MondClock Widget

A minimalist desktop clock widget for Windows that displays the current day and date directly on your desktop wallpaper.

![MondClock Widget](screenshot.png)

## Features

- 📅 **Day & Date Display** - Shows the current day of the week and full date
- 🎨 **Theme Toggle** - Switch between dark (white text) and light (dark text) themes
- 📏 **Three Sizes** - Small, Medium, and Large scaling options
- 🖱️ **Draggable** - Position anywhere on your desktop
- 💾 **Persistent Settings** - Remembers position, size, and theme between sessions
- 🖥️ **Desktop Integration** - Sits on the desktop layer, doesn't interfere with other windows
- 🔒 **Single Instance** - Prevents multiple instances from running

## Requirements

- Windows 10/11
- Visual Studio 2019 or later (for building from source)
- **Font**: BankGothic Md BT (optional - will use system default if not installed)

## Installation

### Option 1: Download Release
1. Download `MondClock.exe` from the [Releases](../../releases) page
2. Place it in a folder where you have write permissions
3. Run `MondClock.exe`

### Option 2: Build from Source
1. Clone this repository
   ```bash
   git clone https://github.com/YOUR_USERNAME/MondClock_Widget.git
   ```
2. Open `MondClock.sln` in Visual Studio
3. Select **Release** configuration and **x64** platform
4. Build the solution (Ctrl+Shift+B)
5. Find the executable in `x64\Release\MondClock.exe`

## Usage

| Action | Description |
|--------|-------------|
| **Left-click + Drag** | Move the widget anywhere on your desktop |
| **Right-click** | Open the settings panel |

### Settings Panel

| Button | Function |
|--------|----------|
| **Small** | Set widget to small size (scale 2x) |
| **Medium** | Set widget to medium size (scale 3x) |
| **Large** | Set widget to large size (scale 4x) |
| **Theme** | Toggle between dark and light text |
| **Exit** | Close the widget |

*Click anywhere outside the settings panel to close it.*

## Configuration

Settings are automatically saved to `mondclock.dat` in the same folder as the executable:
- Window position (X, Y)
- Scale size (2, 3, or 4)
- Theme (dark or light)

## Auto-Start with Windows

To make MondClock start automatically with Windows:
1. Press `Win + R`, type `shell:startup`, and press Enter
2. Create a shortcut to `MondClock.exe` in this folder

## Project Structure

```
MondClock_Widget/
├── MondClock.cpp          # Main source code
├── MondClock.h            # Header file
├── MondClock.rc           # Resource file
├── MondClock.sln          # Visual Studio solution
├── MondClock.vcxproj      # Visual Studio project
├── Resource.h             # Resource definitions
├── framework.h            # Framework includes
├── targetver.h            # Target Windows version
└── README.md              # This file
```

## License

This project is open source. Feel free to use, modify, and distribute.

## Contributing

Contributions are welcome! Feel free to submit issues and pull requests.

---

*Made with ❤️ for Windows desktop customization enthusiasts*
