@echo off
rem ===========================================================================
rem  flash.bat - flashes T1S_Follower onto the SAM E54 board via pyOCD
rem              (EDBG probe, no MPLAB X needed at flash time)
rem
rem  Usage:   flash.bat                  ... flash the committed release\ HEX
rem           flash.bat --dry-run        ... only show what would run
rem           flash.bat <file.hex> ...   ... flash a different image
rem           flash.bat --list           ... list connected probes
rem           flash.bat --probe <serial> ... pick a probe for a single run
rem
rem  Ported from the sister project t1s_100baset_bridge's follower\flash.bat,
rem  simplified: the sister's version selects between "follower_a"/"follower"
rem  roles via a shared ..\scripts\flash_boards.py + ..\json\boards.json
rem  (flashes whichever of two follower boards, by role). This project
rem  flashes a single board directly via pyOCD/flash_same54.py instead - the
rem  same pattern already used by this repo's bridge_lan865x_100baseT
rem  project. Pick the probe explicitly with --probe <serial> if more than
rem  one board is connected.
rem
rem  Defaults to release\T1S_Follower.hex, the HEX build.bat commits after
rem  every successful build (see CLAUDE.md) - so a fresh clone can flash
rem  without building first. To flash a fresh local build instead (e.g.
rem  after editing something), pass the dist\ path explicitly:
rem  flash.bat firmware\T1S_Follower.X\dist\default\production\T1S_Follower.X.production.hex
rem
rem  Uses this project's own .venv (see setup.bat) - falls back to the bare
rem  "python" from PATH if it's missing. All further arguments are passed
rem  through to scripts\flash_same54.py.
rem ===========================================================================
setlocal

set "PROBE="

set "PY=%~dp0.venv\Scripts\python.exe"
if not exist "%PY%" set "PY=python"

set "TOOL=%~dp0scripts\flash_same54.py"
set "HEX=%~dp0release\T1S_Follower.hex"

if not exist "%TOOL%" (
    echo ERROR: %TOOL% missing.
    exit /b 1
)

rem --- first argument: image, unless it's an option -------------------
set "ARGS=%*"
set "FIRST=%~1"
if not "%FIRST%"=="" (
    echo %FIRST% | findstr /b /c:"-" >nul
    if errorlevel 1 (
        set "HEX=%~f1"
        shift
        set "ARGS=%1 %2 %3 %4 %5 %6 %7 %8 %9"
    )
)

rem --- special case --list: no image needed -------------------------------
echo %ARGS% | findstr /c:"--list" >nul
if not errorlevel 1 (
    "%PY%" "%TOOL%" --list
    exit /b %errorlevel%
)

if not exist "%HEX%" (
    echo ERROR: %HEX% does not exist.
    echo         release\ should be tracked in git - check it wasn't deleted, or
    echo         run build.bat once to recreate it.
    exit /b 1
)

echo [flash] Image : %HEX%
echo.

"%PY%" "%TOOL%" "%HEX%" %ARGS%
if errorlevel 1 (
    echo.
    echo ERROR: flashing failed.
    exit /b 1
)

echo.
echo %ARGS% | findstr /c:"--dry-run" >nul
if not errorlevel 1 (
    echo [ok   ] Dry run - the board was not touched.
    exit /b 0
)
echo [ok   ] flashed and reset.
echo         Console: EDBG COM port, 115200 8N1.
exit /b 0
