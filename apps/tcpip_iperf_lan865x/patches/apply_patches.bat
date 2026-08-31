@echo off
rem ===========================================================================
rem  apply_patches.bat - re-applies this project's hand-patches to MCC-generated
rem                       code after a Generate Code / Force Update on All run.
rem
rem  Usage:   apply_patches.bat            ... apply whatever is missing
rem           apply_patches.bat --check    ... dry run, report only
rem
rem  Thin wrapper around apply_patches.py, reusing the sister project's .venv
rem  (same pattern as ..\cli.bat/..\flash.bat) - falls back to the bare "python"
rem  from PATH if that venv is not present on this machine. All arguments are
rem  passed through as-is. See README.md for what this does and doesn't cover.
rem ===========================================================================
setlocal

set "PY=C:\work\t1s_bridge\bridge\t1s_100baset_bridge\.venv\Scripts\python.exe"
if not exist "%PY%" set "PY=python"

"%PY%" "%~dp0apply_patches.py" %*
exit /b %errorlevel%
