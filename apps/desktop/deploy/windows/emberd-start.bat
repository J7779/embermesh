@echo off
rem Starts emberd (EmberMesh node + hub + radio gateway). UNTESTED on Windows.
rem Edit the settings below. Double-click to run, or let install-task.ps1 start it at logon.
setlocal

rem Folder containing emberd.exe (default: the folder of this script)
set "EMBER_DIR=%~dp0"
rem Data dir (blank = default %APPDATA%\EmberMesh)
set "EMBER_DATA="
rem Web UI folder (blank = default search, or built-in fallback page)
set "EMBER_UI=%EMBER_DIR%ui"
rem Extra arguments, e.g. --kiss-serial COM3:9600  or  --kiss-tcp 127.0.0.1:8001
set "EMBER_ARGS="

set "ARGS="
if not "%EMBER_DATA%"=="" set ARGS=%ARGS% --data-dir "%EMBER_DATA%"
if exist "%EMBER_UI%\" set ARGS=%ARGS% --web-root "%EMBER_UI%"

cd /d "%EMBER_DIR%"
"%EMBER_DIR%emberd.exe" %ARGS% %EMBER_ARGS% %*
set RC=%ERRORLEVEL%
if not "%RC%"=="0" (
  echo emberd exited with code %RC%
  rem Non-zero exit lets Task Scheduler's restart-on-failure kick in.
)
exit /b %RC%
