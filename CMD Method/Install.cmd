@echo off
setlocal EnableExtensions EnableDelayedExpansion
title XSISelectFix Installer

set "package_root=%~dp0XSISelectFix"
set "payload_loader=%package_root%\Application\Plugins\XSISelectFixLoader.py"
set "payload_dll=%package_root%\Application\bin\XSISelectFix\XSISelectFix.dll"
set "payload_enable=%package_root%\Application\bin\XSISelectFix\XSISelectFix.enable"

if not exist "%payload_loader%" goto :missing_payload
if not exist "%payload_dll%" goto :missing_payload
if not exist "%payload_enable%" goto :missing_payload

tasklist /fi "imagename eq XSI.exe" 2>nul | find /i "XSI.exe" >nul
if not errorlevel 1 (
  echo Softimage is currently running.
  echo Close every XSI.exe process before installing XSISelectFix.
  pause
  exit /b 1
)

set "profile_count=0"
set "installed_count=0"
set "copy_failed=0"

for /d %%R in ("%USERPROFILE%\Autodesk\Softimage_2015*") do (
  set /a profile_count+=1
  set "target=%%~fR\Addons\XSISelectFix"
  echo Installing to:
  echo !target!
  echo.

  if not exist "%%~fR\Addons" mkdir "%%~fR\Addons" >nul 2>&1
  robocopy "%package_root%" "!target!" /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul
  set "copy_result=!errorlevel!"

  if !copy_result! GEQ 8 (
    set "copy_failed=1"
  ) else (
    if exist "!target!\Application\Plugins\XSISelectFixLoader.py" (
      if exist "!target!\Application\bin\XSISelectFix\XSISelectFix.dll" (
        if exist "!target!\Application\bin\XSISelectFix\XSISelectFix.enable" set /a installed_count+=1
      )
    )
  )
)

if "%profile_count%"=="0" (
  echo No Softimage 2015 user profile was found under:
  echo %USERPROFILE%\Autodesk
  echo.
  echo Start the intended Softimage version once, close it, and run this installer again.
  pause
  exit /b 1
)

echo.
if "%copy_failed%"=="0" if "%installed_count%"=="%profile_count%" (
  echo ============================================================
  echo XSISelectFix was installed successfully.
  echo Installed into %installed_count% Softimage 2015 user profile(s).
  echo.
  echo Start Softimage normally. No injector or Administrator access is needed.
  echo ============================================================
  pause
  exit /b 0
)

echo ============================================================
echo XSISelectFix could not be installed completely.
echo Verified %installed_count% of %profile_count% detected profile(s).
echo Check folder permissions and try again.
echo ============================================================
pause
exit /b 1

:missing_payload
echo The XSISelectFix payload is incomplete.
echo Keep Install-XSISelectFix.cmd beside the supplied XSISelectFix folder.
pause
exit /b 1
