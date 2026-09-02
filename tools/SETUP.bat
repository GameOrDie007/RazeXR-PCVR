@echo off
rem RazeXR PCVR - run this once.
rem
rem Finds the Build games you already own on Steam and GOG, copies their data
rem in, fetches the optional voxel pack, and writes a launcher per game.
rem
rem Nothing is downloaded from us, and no game data ships with this port.
rem
rem   SETUP                 the normal way
rem   SETUP -InPlace        leave the games where they are instead of copying
rem   SETUP -NoDownload     skip the network step
rem   SETUP -Root D:\Games  also search this folder

setlocal
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1" %*

echo.
pause
