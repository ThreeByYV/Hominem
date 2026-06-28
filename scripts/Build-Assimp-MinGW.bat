@echo off
setlocal

set ROOT=%~dp0..
set SRC=%ROOT%\Hominem\vendor\assimp

set MINGW=
where gcc >nul 2>&1 && set MINGW=found_on_path
if not defined MINGW for /d %%D in ("C:\Program Files\JetBrains\CLion*") do set MINGW=%%D\bin\mingw\bin
if not defined MINGW for /d %%D in ("%LOCALAPPDATA%\Programs\clion-*") do set MINGW=%%D\bin\mingw\bin
if not defined MINGW ( echo [Assimp] gcc not found. Add MinGW to PATH or install CLion. & pause & exit /b 1 )
if not "%MINGW%"=="found_on_path" set PATH=%MINGW%;%PATH%

echo [Assimp] Configuring Debug (MinGW / Ninja)...
cmake -S "%SRC%" -B "%SRC%\build\Debug" ^
    -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_C_COMPILER=gcc ^
    -DCMAKE_CXX_COMPILER=g++ ^
    -DASSIMP_BUILD_TESTS=OFF ^
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
    -DASSIMP_BUILD_ZLIB=ON ^
    -DBUILD_SHARED_LIBS=OFF
if %ERRORLEVEL% neq 0 ( echo [Assimp] Configure failed. & pause & exit /b 1 )

echo [Assimp] Building Debug...
cmake --build "%SRC%\build\Debug"
if %ERRORLEVEL% neq 0 ( echo [Assimp] Build failed. & pause & exit /b 1 )

echo [Assimp] Configuring Release (MinGW / Ninja)...
cmake -S "%SRC%" -B "%SRC%\build\Release" ^
    -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER=gcc ^
    -DCMAKE_CXX_COMPILER=g++ ^
    -DASSIMP_BUILD_TESTS=OFF ^
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
    -DASSIMP_BUILD_ZLIB=ON ^
    -DBUILD_SHARED_LIBS=OFF
if %ERRORLEVEL% neq 0 ( echo [Assimp] Configure failed. & pause & exit /b 1 )

echo [Assimp] Building Release...
cmake --build "%SRC%\build\Release"
if %ERRORLEVEL% neq 0 ( echo [Assimp] Build failed. & pause & exit /b 1 )

echo [Assimp] Done. Reload CMake in CLion to pick up the new libs.
pause
endlocal
