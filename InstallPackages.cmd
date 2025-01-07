@echo off
cd ./InstallPackages
msbuild InstallPackages.sln /restore

cd ./InstallPackages
rmdir bin /s /q
rmdir obj /s /q