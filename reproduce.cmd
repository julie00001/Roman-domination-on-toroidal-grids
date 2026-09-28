@echo off
setlocal
rem Private reviewer package. All programs are built from the included source.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0reproduce.ps1" %*
set "reproExit=%errorlevel%"
echo.
if not defined ROMAN_TORUS_NO_PAUSE pause
exit /b %reproExit%
