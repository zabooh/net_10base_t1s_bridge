@echo off
rem ===========================================================================
rem  run_gui_telnet.bat - launches scripts\bridge_gui_telnet.py (Bridge Status &
rem                Configuration GUI, Telnet variant) against the board's
rem                Telnet server (TCP/23) instead of the EDBG virtual COM port.
rem
rem  Parallel to run_gui.bat/bridge_gui.py - same tool, connection layer swapped.
rem  Reuses the sister project's .venv (sv-ttk already installed there) instead
rem  of setting up a second one - same pattern as run_gui.bat - falls back to
rem  the bare "python" from PATH if that venv is not present on this machine.
rem ===========================================================================
setlocal

set "SCRIPT_DIR=%~dp0"
set "PY=C:\work\t1s_bridge\bridge\t1s_100baset_bridge\.venv\Scripts\python.exe"
if not exist "%PY%" set "PY=python"

"%PY%" --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found.
    pause
    exit /b 1
)

echo Starting Bridge GUI (Telnet)...
cd /d "%SCRIPT_DIR%"
"%PY%" scripts\bridge_gui_telnet.py

if errorlevel 1 (
    echo.
    echo ERROR: GUI failed to start
    pause
    exit /b 1
)
