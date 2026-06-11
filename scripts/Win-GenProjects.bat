@echo off

echo.
echo Generating Visual Studio 2026 solution...
echo.

pushd ..\
call vendor\premake\premake5.exe vs2026
popd

echo.
echo Done! Open Hominem.slnx in Visual Studio 2026
echo.
PAUSE
