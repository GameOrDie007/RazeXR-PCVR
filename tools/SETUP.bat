@echo off
rem RazeXR PCVR - install or update.
rem
rem Finds the Build games you already own (Steam, GOG, the 3D Realms Anthology),
rem installs RazeXR PCVR to Documents\RazeXR PCVR, copies the games' data in,
rem fetches the optional voxel packs, and writes a launcher per game. Run from
rem a folder that is already set up (an older version you extracted this over),
rem it updates that folder where it is.
rem
rem Nothing is downloaded from us, and no game data ships with this port.
rem
rem   SETUP                 the normal way
rem   SETUP -InPlace        leave the games where they are instead of copying
rem   SETUP -NoDownload     skip the network step
rem   SETUP -Root D:\Games  also search this folder
rem   SETUP "<folder>"      an older RazeXR PCVR folder: update that one

setlocal
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1" %*

echo.
pause
