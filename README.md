# Reach

> A Direct2d based lightweight Windows 11 shell replacement.

## Screenshots

<p>
<img src="./resources/readme/reach-desktop.png" alt="Reach desktop" width="50%"><img src="./resources/readme/reach-launcher.png" alt="Reach launcher" width="50%">
<img src="./resources/readme/reach-quick-settings.png" alt="Reach quick settings" width="50%"><img src="./resources/readme/reach-tray.png" alt="Reach tray" width="50%">
</p>

## Important notice

Reach replaces Windows Explorer as the shell, which means that Explorer does not run in the background. This breaks a few apps that rely on it, most importantly Settings. I haven't tested other Windows Store apps as I don't use them.

## Important hotkeys

In case of an emergency, you can always open Task manager (CTRL + SHIFT + ESC), click on run, type powershell, then press CTRL + SHIFT + ENTER. This will open an elevated powershell session, and you can follow the instructions below to restore explorer.

## Requirements

Microsoft Visual C++ Redistributable for Visual Studio 2015–2022 (x64)
[Everything by voidtools](https://www.voidtools.com/) to be installed and running.

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
reachctl --install
```

This configures Windows to launch Reach instead of Explorer, effective starting from the next Windows session.
Then, to start Reach immediately for your current session, run:

```powershell
reachctl --start
```

You also need Voidtools' Everything installed and running to use the launcher feature.

In case of a problem, you can reset Windows Explorer as the shell by running this as admin:

```powershell
reachctl --reset
```

## License

MIT — see [LICENSE](./LICENSE) for details.
