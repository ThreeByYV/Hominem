@echo off
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Hominem.sln /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal > build_output.txt 2>&1
