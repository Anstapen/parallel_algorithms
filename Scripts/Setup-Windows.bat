@echo off

pushd ..
Vendor\Binaries\Premake\Windows\premake5.exe --file=Build.lua vs2022 --graphics=opengl43
popd
pause