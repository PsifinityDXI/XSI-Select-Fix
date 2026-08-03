@echo off
setlocal EnableExtensions
title XSISelectFix Standalone Add-On Builder
cd /d "%~dp0"

where py >nul 2>&1
if not errorlevel 1 (
  py -3 "%~dp0build-xsiaddon-standalone.py"
  set "result=%errorlevel%"
  goto :finished
)

where python >nul 2>&1
if not errorlevel 1 (
  python "%~dp0build-xsiaddon-standalone.py"
  set "result=%errorlevel%"
  goto :finished
)

echo Python 3 was not found.
echo Install Python 3 or run build-xsiaddon-standalone.py with a Python 3 interpreter.
set "result=1"

:finished
echo.
if "%result%"=="0" (
  echo XSISelectFix.xsiaddon is ready in the dist folder.
) else (
  echo The XSISelectFix.xsiaddon build failed.
)
pause
exit /b %result%
