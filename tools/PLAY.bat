@echo off
rem RazeXR PCVR - 1:1 build.
rem
rem   PLAY            Duke Nukem 3D
rem   PLAY blood      Blood
rem   PLAY sw         Shadow Warrior
rem   PLAY exhumed    Exhumed / Powerslave
rem   PLAY redneck    Redneck Rampage
rem   PLAY nam        NAM
rem   PLAY ww2gi      World War II GI
rem
rem Start Virtual Desktop and connect the headset BEFORE running this.
rem
rem -nosetup skips Raze's game picker. Without it, any game whose folder holds
rem more than one candidate (Blood has several add-on zips) stops on a launcher
rem window and waits for a click, which from outside looks exactly like a hang.
rem
rem Two logs are written beside this script:
rem   raze.log        the engine's own startup log
rem   razexr_vr.log   the VR layer, including everything printed before the
rem                   console exists. Read this one first if it misbehaves.
setlocal
cd /d "%~dp0"
set "BASE=E:\Games\Quest Ports\RazeXR\raze"

set "GAME=%~1"
if "%GAME%"=="" set "GAME=duke"

if /i "%GAME%"=="duke"    set "GRP=%BASE%\duke\DUKE3D.GRP"
if /i "%GAME%"=="blood"   set "GRP=%BASE%\blood\BLOOD.RFF"
if /i "%GAME%"=="sw"      set "GRP=%BASE%\shadowwarrior\Sw.grp"
if /i "%GAME%"=="exhumed" set "GRP=%BASE%\exhumed\STUFF.DAT"
if /i "%GAME%"=="redneck" set "GRP=%BASE%\rampage\REDNECK.GRP"
if /i "%GAME%"=="nam"     set "GRP=%BASE%\nam\NAM.GRP"
if /i "%GAME%"=="ww2gi"   set "GRP=%BASE%\ww2gi\WW2GI.GRP"

if not defined GRP (
    echo Unknown game "%GAME%".
    echo Use one of: duke blood sw exhumed redneck nam ww2gi
    exit /b 1
)

del "%~dp0razexr_vr.log" 2>nul
"%~dp0raze.exe" -nosetup -gamegrp "%GRP%" -config "%~dp0cfg_%GAME%.ini" +logfile "%~dp0raze.log"
