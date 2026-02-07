# PhysX SDK - vcpkg Auto-Install

PhysX is automatically installed via **vcpkg** (Microsoft's C++ package manager).

## Quick Start

Just run:
```batch
scripts\Win-GenProjects.bat
```

**First run:** Premake will automatically:
1. ✅ Install vcpkg (if not present)
2. ✅ Download PhysX via vcpkg (pre-built binaries!)
3. ✅ Integrate with Visual Studio
4. ✅ Generate your solution

**Takes ~5-15 minutes first time. Instant after that!**

## Why vcpkg?

- ✅ **Pre-built binaries** - No complex building from source
- ✅ **Automatic** - Handles all dependencies
- ✅ **Integrated** - Works seamlessly with Visual Studio
- ✅ **Reliable** - Microsoft-maintained packages

## Requirements

- Git
- Visual Studio 2022 with C++ build tools
- ~2GB disk space for vcpkg cache

## How It Works

vcpkg installs PhysX to: `Hominem/vendor/vcpkg/installed/x64-windows/`

Libraries are automatically linked through vcpkg's Visual Studio integration - no manual configuration needed!

## Manual Reinstall

To reinstall PhysX:
```batch
rmdir /s /q Hominem\vendor\vcpkg
scripts\Win-GenProjects.bat
```

## Documentation

- [vcpkg Documentation](https://github.com/microsoft/vcpkg)
- [PhysX Documentation](https://nvidia-omniverse.github.io/PhysX/)
