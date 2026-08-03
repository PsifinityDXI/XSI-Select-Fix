@echo off
setlocal
where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake was not found. Install Visual Studio 2022 with Desktop development with C++ and CMake tools.
  exit /b 1
)

set "source_dir=%~dp0."
set "build_dir=%~dp0build"

cmake -S "%source_dir%" -B "%build_dir%" -A x64
if errorlevel 1 exit /b 1

cmake --build "%build_dir%" --config Release
if errorlevel 1 exit /b 1

echo.
echo Build completed. Files are in:
echo %~dp0build\Release
endlocal
