-- Dependencies module for automatic dependency management

Dependencies = {}

local function ExecuteCommand(cmd)
    local result = os.execute(cmd)
    return result == 0 or result == true
end

function Dependencies.InstallVcpkg()
    local vcpkgPath = "Hominem/vendor/vcpkg"

    if os.isdir(vcpkgPath) then
        print("  vcpkg already installed")
        return true
    end

    print("\n[Installing vcpkg package manager...]")

    if not ExecuteCommand('git clone https://github.com/microsoft/vcpkg.git "' .. vcpkgPath .. '"') then
        print("Failed to clone vcpkg")
        return false
    end

    if not ExecuteCommand('call "' .. vcpkgPath .. '\\bootstrap-vcpkg.bat"') then
        print("Failed to bootstrap vcpkg")
        return false
    end

    print("  vcpkg installed successfully")
    return true
end

function Dependencies.DownloadAndBuildPhysX(vendorPath)
    print("\nSetting up PhysX via vcpkg...")
    print("  This will download and build PhysX (5-15 minutes)")

    -- Check for Git
    if not ExecuteCommand("git --version >nul 2>&1") then
        print("\nERROR: Git not found. Please install Git and try again.")
        print("   Download from: https://git-scm.com/download/win")
        return false
    end

    -- Install vcpkg if needed
    if not Dependencies.InstallVcpkg() then
        return false
    end

    local vcpkgExe = "Hominem\\vendor\\vcpkg\\vcpkg.exe"

    -- Install PhysX via vcpkg
    print("\n[Installing PhysX via vcpkg...]")
    print("  This downloads pre-built binaries - much faster than building from source!")

    if not ExecuteCommand(vcpkgExe .. ' install physx:x64-windows') then
        print("Failed to install PhysX via vcpkg")
        return false
    end

    -- Integrate with Visual Studio
    print("\n[Integrating vcpkg with Visual Studio...]")
    ExecuteCommand(vcpkgExe .. ' integrate install')

    print("\nPhysX installed successfully via vcpkg!")
    print("Note: Using vcpkg-installed PhysX. No manual library paths needed.")
    print("")

    return true
end

function Dependencies.SetupPhysX(vendorPath)
    local vcpkgPath = vendorPath .. "/vcpkg"
    local vcpkgPhysX = vcpkgPath .. "/installed/x64-windows/include/PxPhysicsAPI.h"

    -- Check if PhysX is installed via vcpkg
    if os.isfile(vcpkgPhysX) then
        print("PhysX found (vcpkg)")
        return true
    end

    -- PhysX not found - auto-install via vcpkg
    print("PhysX not found")
    print("Auto-installing via vcpkg...")
    return Dependencies.DownloadAndBuildPhysX(vendorPath)
end

return Dependencies
