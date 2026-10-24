# Launcher for Shield/Project-BO4 Client
## Installation
- #### [Download](https://github.com/NotNierPea/shield-launcher/releases/tag/Release), then extract it in the ``"Call of Duty Black Ops 4"`` directory (wherever you installed the game)
- #### Then you can run the ``"Launch Project BO4.exe"``, Click offline or online to launch the game.

## Linux
The launcher is cross-platform: the same sources build for Windows and Linux.

```
premake5 vs2022     # Windows, MSVC, vendored Qt6 from deps/qt6
premake5 gmake      # Linux, system Qt6
```

On Linux, use the helper script:

```sh
./build-linux.sh            # release
./build-linux.sh debug      # debug
```

Put the resulting `Shield_Launcher` binary in `project-bo4/launcher/`, next to
`solo.zip`, `mp.zip` and `exe.zip`, then start it directly.

The game runs through Proton or Wine. The launcher shows a runtime list built
from Steam, `compatibilitytools.d`, Heroic and umu, and a `Prefix...` button:
pick an existing prefix and it is used, pick an empty folder and the prefix is
created there.

Requirements: a C++20 compiler, Qt6 base/widgets/multimedia development
packages, premake5. Proton launching uses [umu-launcher](https://github.com/Open-Wine-Components/umu-launcher)
(`umu-run`), which works with any Proton build including GE-Proton.


## Info
- #### The launcher was not developed by me, i just maintain it and improve it.
- #### Also contains the support mod files and dlls for the latest version of client. 
- #### For any help or troubleshooting, join our [discord server](https://discord.gg/AXECAzJJGU).
- #### [Shield Documentation](https://shield-client.gitbook.io/shield-documentation)

![image](https://github.com/NotNierPea/shield-launcher/blob/main/project-bo4/Latest-Release-Files/project-bo4/launcher/images/launcher.png)
