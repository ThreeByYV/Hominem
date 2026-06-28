@echo off
setlocal

set ROOT=%~dp0..
set SRC=%ROOT%\Hominem\vendor\assimp
set BUILD=%SRC%\build

echo [Assimp] Clearing stale CMake cache...
if exist "%BUILD%\CMakeCache.txt" del /f /q "%BUILD%\CMakeCache.txt"
if exist "%BUILD%\CMakeFiles" rd /s /q "%BUILD%\CMakeFiles"

echo [Assimp] Configuring with Visual Studio 18 2026...
cmake -S "%SRC%" -B "%BUILD%" ^
    -G "Visual Studio 18 2026" -A x64 ^
    -DASSIMP_BUILD_TESTS=OFF ^
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
    -DASSIMP_BUILD_SAMPLES=OFF ^
    -DASSIMP_INSTALL=OFF ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DCMAKE_DEBUG_POSTFIX=d ^
    -DLIBRARY_SUFFIX=""
if %ERRORLEVEL% neq 0 ( echo [Assimp] Configure failed. & pause & exit /b 1 )

echo [Assimp] Building Debug...
cmake --build "%BUILD%" --config Debug --target assimp
if %ERRORLEVEL% neq 0 ( echo [Assimp] Debug build failed. & pause & exit /b 1 )

echo [Assimp] Building Release...
cmake --build "%BUILD%" --config Release --target assimp
if %ERRORLEVEL% neq 0 ( echo [Assimp] Release build failed. & pause & exit /b 1 )

echo [Assimp] Renaming libs to predictable names...

for /f "delims=" %%i in ('dir /b "%BUILD%\lib\Debug\assimp*.lib" 2^>nul') do (
    copy /y "%BUILD%\lib\Debug\%%i" "%BUILD%\lib\Debug\assimpd.lib" >nul
    echo   Debug:   %%i -^> assimpd.lib
)

for /f "delims=" %%i in ('dir /b "%BUILD%\lib\Release\assimp*.lib" 2^>nul') do (
    copy /y "%BUILD%\lib\Release\%%i" "%BUILD%\lib\Release\assimp.lib" >nul
    echo   Release: %%i -^> assimp.lib
)

echo.
echo [Assimp] Done.
pause
endlocal
