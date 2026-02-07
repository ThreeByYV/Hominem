@echo off

echo.
echo Generating Visual Studio 2022 solution...
echo.

pushd ..\
call vendor\premake\premake5.exe vs2022
popd

echo.
echo Done! Open Hominem.sln in Visual Studio 2022
echo.
PAUSE
