@echo off
setlocal EnableExtensions
title Build XSISelectFix - XSIAddon Method
cd /d "%~dp0Source"
call build-release.cmd
if errorlevel 1 goto :failed
where py >nul 2>&1
if errorlevel 1 goto :python
py -3 build-xsiaddon-standalone.py --output "%~dp0XSISelectFix.xsiaddon"
if errorlevel 1 goto :failed
echo XSIAddon Method is ready. Run Install.cmd normally.
pause
exit /b 0
:python
echo Python 3 was not found.
:failed
echo Build failed.
pause
exit /b 1
