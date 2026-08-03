@echo off
setlocal EnableExtensions EnableDelayedExpansion
title XSISelectFix Uninstaller

tasklist /fi "imagename eq XSI.exe" 2>nul | find /i "XSI.exe" >nul
if not errorlevel 1 (
  echo Softimage is currently running.
  echo Close every XSI.exe process before removing XSISelectFix.
  pause
  exit /b 1
)

set "found_addon=0"
set "remove_failed=0"
for /d %%R in ("%USERPROFILE%\Autodesk\Softimage_2015*") do (
  set "candidate=%%~fR\Addons\XSISelectFix"
  if exist "!candidate!\Application\Plugins\XSISelectFixLoader.py" (
    if exist "!candidate!\Application\bin\XSISelectFix\XSISelectFix.dll" (
      set "found_addon=1"
      echo Removing:
      echo !candidate!
      rmdir /s /q "!candidate!"
      if exist "!candidate!" set "remove_failed=1"
    )
  )

  rem Softimage normally installs the expanded payload under Addons, but some
  rem installation paths also retain the original package in InstalledAddons.
  rem Remove only exact XSISelectFix package names; never sweep that directory.
  for %%I in (
    "%%~fR\Addons\InstalledAddons\XSISelectFix.xsiaddon"
    "%%~fR\InstalledAddons\XSISelectFix.xsiaddon"
  ) do if exist "%%~fI" (
    set "found_addon=1"
    echo Removing:
    echo %%~fI
    del /f /q "%%~fI"
    if exist "%%~fI" set "remove_failed=1"
  )

  for %%I in (
    "%%~fR\Addons\InstalledAddons\XSISelectFix"
    "%%~fR\InstalledAddons\XSISelectFix"
  ) do if exist "%%~fI\" (
    set "found_addon=1"
    echo Removing:
    echo %%~fI
    rmdir /s /q "%%~fI"
    if exist "%%~fI\" set "remove_failed=1"
  )
)

echo.
if "%found_addon%"=="0" (
  echo XSISelectFix is not installed for this user.
  pause
  exit /b 0
)

if "%remove_failed%"=="1" (
  echo XSISelectFix could not be removed completely.
  echo Check folder permissions and confirm that every XSI.exe process is closed.
  pause
  exit /b 1
)

echo ============================================================
echo XSISelectFix was removed successfully.
echo ============================================================
pause
exit /b 0
