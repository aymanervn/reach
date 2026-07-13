# Reach

> A Direct2d based lightweight Windows 11 shell replacement.

## Screenshots

<p>
<img src="./resources/readme/reach-quick-settings.jpg" alt="Reach quick settings" width="50%"><img src="./resources/readme/reach-windows-updates.jpg" alt="Reach Windows updates" width="50%">
<img src="./resources/readme/reach-launcher.jpg" alt="Reach launcher and clipboard" width="50%"><img src="./resources/readme/reach-split-screen.png" alt="Reach split screen" width="50%">
</p>

## Important notice

Reach replaces Windows Explorer, which will not be running in the background. This breaks a few apps that rely on it, most importantly Windows settings. I haven't tested other Windows Store apps as I don't use them.

## Important hotkeys

In case of an emergency, you can always open Task manager (CTRL + SHIFT + ESC), click on run, type powershell, then press CTRL + SHIFT + ENTER. This will open an elevated powershell session, and you can follow the instructions below to restore explorer.

## Features

- Animated wallpapers through Wallpaper Engine.
- Split screen: WIN + arrow keys snaps the focused window, pressing again maximizes it. Works both horizontally and vertically.
- Now playing controls on the dock for whatever media is playing.
- Clipboard history with text and image previews.
- Windows security updates can be checked and installed from Reach's own settings.
- No distractions: no ads, no widgets, no news. Reach also purposefully stops rendering its UI during a game session, and the game gets minimized on alt tab.

## Reach app launcher

Press the windows key to open the app launcher and the clipboard history, it uses voidtools' Everything SDK and can search any file on the NTFS disks.

## Requirements

- Microsoft Visual C++ Redistributable for Visual Studio 2015–2022 (x64)
- [Everything by voidtools](https://www.voidtools.com/) neeed to be installed and running.

## Build

To build Reach, run:

```powershell
cmake -B build
cmake --build build --config Release --target reach_release_zip
```

This produces a zip file with the distributables.

## Installation

Run as admin:

```powershell
./reachctl --install
```

This configures Windows to launch Reach instead of Explorer, effective starting from the next Windows session.
Then, to start Reach immediately for your current session, run:

```powershell
./reachctl --start
```

You also need Voidtools' Everything installed and running to use the launcher feature.

In case of a problem, you can reset Windows Explorer as the shell by running this as admin:

```powershell
./reachctl --reset
```

## License

MIT — see [LICENSE](./LICENSE) for details.
