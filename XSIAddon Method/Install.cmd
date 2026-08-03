@echo off
setlocal EnableExtensions EnableDelayedExpansion
title XSISelectFix Installer

set "addon_path=%~dp0XSISelectFix.xsiaddon"
if not exist "%addon_path%" if exist "%~dp0dist\XSISelectFix.xsiaddon" set "addon_path=%~dp0dist\XSISelectFix.xsiaddon"

if not exist "%addon_path%" (
  echo XSISelectFix.xsiaddon was not found beside this installer or in dist.
  echo Run build-xsiaddon.py from Softimage's Script Editor first.
  pause
  exit /b 1
)

tasklist /fi "imagename eq XSI.exe" 2>nul | find /i "XSI.exe" >nul
if not errorlevel 1 (
  echo Softimage is currently running.
  echo Close every XSI.exe process before installing XSISelectFix.
  pause
  exit /b 1
)

set "xsi_root=%~1"
if not defined xsi_root if exist "C:\Program Files\Autodesk\Softimage 2015 SP2\Application\bin\XSI.exe" set "xsi_root=C:\Program Files\Autodesk\Softimage 2015 SP2"
if not defined xsi_root if exist "C:\Program Files\Autodesk\Softimage 2015 R2 SP2\Application\bin\XSI.exe" set "xsi_root=C:\Program Files\Autodesk\Softimage 2015 R2 SP2"
if not defined xsi_root if exist "C:\Program Files\Autodesk\Softimage 2015\Application\bin\XSI.exe" set "xsi_root=C:\Program Files\Autodesk\Softimage 2015"

if not defined xsi_root (
  echo Softimage was not found in a standard location.
  echo Usage: Install-XSISelectFix.cmd "C:\Program Files\Autodesk\Softimage 2015 SP2"
  pause
  exit /b 1
)

set "xsi_exe=%xsi_root%\Application\bin\XSI.exe"
set "xsi_setenv=%xsi_root%\Application\bin\setenv.bat"
if not exist "%xsi_exe%" (
  echo XSI.exe was not found at:
  echo %xsi_exe%
  pause
  exit /b 1
)
if not exist "%xsi_setenv%" (
  echo Softimage's environment script was not found at:
  echo %xsi_setenv%
  pause
  exit /b 1
)

set "SI_HOME=%xsi_root%"
call "%xsi_setenv%"
if errorlevel 1 (
  echo Softimage's command environment could not be initialized.
  pause
  exit /b 1
)

echo Installing:
echo %addon_path%
echo.
start "" /wait "%xsi_exe%" -i "%addon_path%"
set "install_result=%errorlevel%"

rem Softimage 2015 can return 0xC0000005 after completing a headless add-on
rem installation. Verify the installed payload instead of trusting that code.
set "installed_dir="
for /d %%R in ("%USERPROFILE%\Autodesk\Softimage_2015*") do (
  set "candidate=%%~fR\Addons\XSISelectFix"
  if exist "!candidate!\Application\Plugins\XSISelectFixLoader.py" (
    if exist "!candidate!\Application\bin\XSISelectFix\XSISelectFix.dll" (
      if exist "!candidate!\Application\bin\XSISelectFix\XSISelectFix.enable" set "installed_dir=!candidate!"
    )
  )
)

echo.
if defined installed_dir (
  echo ============================================================
  echo XSISelectFix was installed successfully.
  echo.
  echo Installed at:
  echo !installed_dir!
  if not "%install_result%"=="0" (
    echo.
    echo Note: Softimage returned code %install_result% after writing all files.
    echo The installed payload passed verification, so this code was ignored.
  )
  echo.
  echo Start Softimage normally. No injector or Administrator access is needed.
  echo ============================================================
  pause
  exit /b 0
)

echo ============================================================
echo XSISelectFix could not be verified after installation.
echo Softimage returned code %install_result%.
echo.
echo No complete XSISelectFix payload was found under:
echo %USERPROFILE%\Autodesk\[Softimage 2015 user folder]\Addons\XSISelectFix
echo ============================================================
pause
exit /b 1
