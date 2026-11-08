# Smooth Headtracking

F4SE plugin for Fallout 4 that makes your character's head and upper body smoothly follow your aim in third person, more like Starfield than Oblivion.

**Nexus:** https://www.nexusmods.com/fallout4/mods/107784

## Features

- Head, neck and upper spine smoothly follow where you aim
- **Look at Camera**: when you look at your character from the front, they look back at you
- Turns off by itself in dialogue, VATS, furniture, first person and iron sights
- Optional: idle timeout, toggle key, disable with weapon drawn
- Fully configurable through MCM or INI

## Requirements

- [F4SE](https://f4se.silverlock.org)
- [Address Library for F4SE Plugins](https://www.nexusmods.com/fallout4/mods/47327)
- [MCM](https://www.nexusmods.com/fallout4/mods/21497) (optional)

Supports Old-Gen (1.10.163), Next-Gen (1.10.984) and Anniversary Edition (1.11.191+) with a single DLL.

## Building

Requirements:
- Visual Studio 2026 with **Desktop development with C++** (includes vcpkg)
- [CMake](https://cmake.org) 4.3 or newer
- `VCPKG_ROOT` environment variable pointing to your vcpkg folder

```powershell
git clone --recurse-submodules https://github.com/Kechu1/Smooth-Headtracking.git
cd Smooth-Headtracking
cmake --preset build-release-msvc-msvc
cmake --build --preset release-msvc-msvc
```

## Credits

- [CommonLibF4 AV](https://github.com/LucaDotGit/CommonLibF4) by LucaDotGit, based on [CommonLibF4](https://github.com/Ryan-rsm-McKenzie/CommonLibF4) by Ryan McKenzie and [CommonLibF4 NG](https://github.com/alandtse/CommonLibF4) by alandtse
- [fmt](https://github.com/fmtlib/fmt), [spdlog](https://github.com/gabime/spdlog), [SimpleIni](https://github.com/brofield/simpleini)

Made by Kechu.
