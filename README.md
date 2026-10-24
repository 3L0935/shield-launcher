# Launcher for Shield/Project-BO4 Client
## Installation

1. Download the release for your platform:
   - **Windows**: `Project_BO4_Launcher_Windows.zip`
   - **Linux**: `Project_BO4_Launcher_Linux.tar.gz`
2. Extract the archive somewhere.
3. Drag what is inside the folder into your `Call of Duty Black Ops 4` directory
   (the one with `BlackOps4.exe`):
   - `Shield_Launcher.exe` on Windows (+ its Qt DLLs and `platforms/`), or
     `Shield_Launcher` on Linux, at the game root
   - the `project-bo4` folder next to it (resources the launcher loads)
4. Start `Shield_Launcher` and pick Offline or Online.

That is the whole install: the launcher ends up at the game root, its resources
under `project-bo4/launcher`, and it finds them on its own.

### Linux

The game runs through Proton or Wine. The launcher lists the Proton/Wine builds
it finds in Steam, `compatibilitytools.d`, Heroic and umu, and a `Prefix...`
button lets you pick the wine prefix:
- pick a folder that already is a prefix (one holding Battle.net is proposed
  automatically) and it is used as is
- pick an empty folder and the prefix is created there

Requirements: a C++20 compiler, Qt6 base/widgets/multimedia development packages,
premake5 to build, and [umu-launcher](https://github.com/Open-Wine-Components/umu-launcher)
(`umu-run`) to run the game with any Proton build, GE-Proton included.

Build it yourself:

```sh
./build-linux.sh            # release
./build-linux.sh debug      # debug
# or, by hand:
premake5 gmake              # Linux, system Qt6
premake5 vs2022             # Windows, MSVC, vendored Qt6 from deps/qt6
```

## Info
- #### The launcher was not developed by me, i just maintain it and improve it.
- #### Also contains the support mod files and dlls for the latest version of client. 
- #### For any help or troubleshooting, join our [discord server](https://discord.gg/AXECAzJJGU).
- #### [Shield Documentation](https://shield-client.gitbook.io/shield-documentation)

![image](https://github.com/NotNierPea/shield-launcher/blob/main/project-bo4/Latest-Release-Files/project-bo4/launcher/images/launcher.png)
