@echo off
setlocal EnableExtensions
title Build XSISelectFix - CMD Method
cd /d "%~dp0Source"
call build-release.cmd
if errorlevel 1 goto :failed
set "payload=%~dp0XSISelectFix"
mkdir "%payload%\Application\bin\XSISelectFix" 2>nul
mkdir "%payload%\Application\Plugins" 2>nul
copy /y "build\Release\XSISelectFix.dll" "%payload%\Application\bin\XSISelectFix\" >nul
copy /y "build\Release\XSISelectFix.enable" "%payload%\Application\bin\XSISelectFix\" >nul
copy /y "startup\XSISelectFixLoader.py" "%payload%\Application\Plugins\" >nul
echo CMD Method is ready. Run Install.cmd normally.
pause
exit /b 0
:failed
echo Build failed.
pause
exit /b 1
