@echo off
IF NOT EXIST .\vcpkg\ (
    git clone https://github.com/microsoft/vcpkg
    .\vcpkg\bootstrap-vcpkg.bat
)
.\vcpkg\vcpkg --overlay-ports=vcpkg_ports remove gamenetworkingsockets --triplet=x64-windows
rmdir /s /q  %LOCALAPPDATA%\vcpkg\archives
rmdir /s /q  .\vcpkg\buildtrees
.\vcpkg\vcpkg --overlay-ports=vcpkg_ports install gamenetworkingsockets --triplet=x64-windows
