@echo off
setlocal
:: ===========================================================================
:: setup.bat - the ONE script to run once, per machine, after cloning.
::
:: Adapts the project to the local machine: Python venv + deps, pyOCD (for
:: flashing), and the SAME54_DFP debug fix. It drives batch\setup_venv.bat /
:: install.bat / batch\genmk.bat for you - those exist as separate files
:: because other scripts also call them, but you should never need to run
:: them directly yourself. After this:
::
::   build.bat
::   flash.bat
::
:: The only other script you run directly, and only when it applies:
:: switching which board flash.bat programs, via "install.bat --select" (see
:: 2/4 below).
::
:: Connect the board via its USB debugger port BEFORE running this so the
:: probe check can detect it. Steps are independent: a failure in one is
:: reported but does not abort the rest.
::
:: Ported from this repo's own bridge_lan865x_100baseT project's setup.bat
:: (itself ported from the sister project t1s_100baset_bridge). Same
:: simplification: no compiler-selection step (setup_compiler.py feeds
:: build_summary.py, which neither this project nor the bridge one has),
:: and no multi-board role-based flashing (flash_boards.py/boards.json) -
:: this is a single, self-contained project, see flash.bat.
:: ===========================================================================
set "SCRIPT_DIR=%~dp0"
set "RC=0"

echo ============================================================
echo   T1S_Follower (PTP-over-T1S endpoint) - one-time machine setup
echo ============================================================

where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python not found in PATH. Install Python 3.9+ and re-run.
    exit /b 1
)

echo.
echo [1/4] Python virtual environment ^(.venv^) + dependencies ...
call "%SCRIPT_DIR%batch\setup_venv.bat"
if errorlevel 1 ( echo [WARN] .venv setup failed - check your network/pip. & set "RC=1" )

rem Resolved AFTER step 1, which is what creates .venv on a fresh clone - every
rem other .bat in this repo can assume .venv already exists and resolve PY near
rem the top, but this script cannot. Falls back to the bare "python" from PATH
rem if .venv is still missing (step 1 failed), so step 3 still gets attempted.
set "PY=%SCRIPT_DIR%.venv\Scripts\python.exe"
if not exist "%PY%" set "PY=python"

echo.
echo [2/4] Flasher prerequisites (pyOCD, EDBG probe, SAME54_DFP pack) ...
call "%SCRIPT_DIR%install.bat" --install
if errorlevel 1 ( echo [WARN] install.bat reported missing prerequisites - see above. & set "RC=1" )

echo.
echo [3/4] VS Code debug fix (SAME54_DFP tool pack) ...
"%PY%" "%SCRIPT_DIR%scripts\setup_debug.py"
if errorlevel 1 ( echo [WARN] setup_debug.py failed - only needed for VS Code debugging. & set "RC=1" )

rem The nbproject Makefile fragments are gitignored - they carry absolute paths
rem of the machine that generated them, so a fresh clone has none. Generating
rem them here means the first build.bat has nothing left to discover; build.bat
rem does it too, so this step is a convenience, not a prerequisite. This
rem project has no MCC model (see firmware\T1S_Follower.X\KEIN_MCC_MODELL.md) -
rem genmk.bat only regenerates the Makefile fragments from the tracked
rem nbproject\configurations.xml, it never touches firmware\src\config\default\.
echo.
echo [4/4] MPLAB X project Makefiles (no IDE session needed) ...
call "%SCRIPT_DIR%batch\genmk.bat" "%SCRIPT_DIR%firmware\T1S_Follower.X"
if errorlevel 1 ( echo [WARN] genmk.bat failed - build.bat will try again. & set "RC=1" )

echo.
echo ============================================================
if "%RC%"=="0" (
    echo   Setup complete. Now build and flash:
) else (
    echo   Setup finished with warnings ^(see above^). You can still:
)
echo     build.bat
echo     flash.bat
echo ============================================================
endlocal & exit /b %RC%
