@echo off
setlocal EnableDelayedExpansion
:: ============================================================================
::  package\build_and_package.bat
::
::  Full Windows build → windeployqt → Inno Setup pipeline.
::
::  Usage:
::    build_and_package.bat [Release|Debug] [path\to\Qt6\root]
::
::  Examples:
::    build_and_package.bat
::    build_and_package.bat Release "C:\Qt\6.7.3\msvc2022_64"
::
::  Requirements
::  ------------
::  - CMake 3.20+  on PATH
::  - MSVC 2022 (x64) or MinGW-w64 toolchain
::  - Qt 6.2+ with windeployqt6.exe in <Qt>\bin\
::  - Inno Setup 6 installed to default path OR iscc.exe on PATH
:: ============================================================================

:: ── Config ──────────────────────────────────────────────────────────────────
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release

set QT_ROOT=%~2
if "%QT_ROOT%"=="" set QT_ROOT=C:\Qt\6.7.3\msvc2022_64

set BUILD_DIR=%~dp0..\build
set DEPLOY_DIR=%BUILD_DIR%\deploy
set DIST_DIR=%~dp0..\dist
set SCRIPT_DIR=%~dp0

:: Inno Setup default install locations
set ISCC=%ProgramFiles(x86)%\Inno Setup 6\iscc.exe
if not exist "%ISCC%" set ISCC=%ProgramFiles%\Inno Setup 6\iscc.exe
if not exist "%ISCC%" (
    where iscc >nul 2>&1 && set ISCC=iscc
)

:: ── Banner ──────────────────────────────────────────────────────────────────
echo.
echo  ============================================================
echo   AVRDUDE GUI  –  Windows Build ^& Package
echo   Config   : %CONFIG%
echo   Qt root  : %QT_ROOT%
echo   Build dir: %BUILD_DIR%
echo  ============================================================
echo.

:: ── Step 1: CMake configure ─────────────────────────────────────────────────
echo [1/5] CMake configure...
cmake -B "%BUILD_DIR%" ^
      -DCMAKE_BUILD_TYPE=%CONFIG% ^
      -DQt6_DIR="%QT_ROOT%\lib\cmake\Qt6" ^
      "%SCRIPT_DIR%.."
if errorlevel 1 ( echo ERROR: CMake configure failed & exit /b 1 )

:: ── Step 2: Build ───────────────────────────────────────────────────────────
echo.
echo [2/5] Building adgui.exe (%CONFIG%)...
cmake --build "%BUILD_DIR%" --config %CONFIG% --parallel
if errorlevel 1 ( echo ERROR: Build failed & exit /b 1 )

:: ── Step 3: windeployqt staging ─────────────────────────────────────────────
echo.
echo [3/5] Staging deployment directory with windeployqt...
set WINDEPLOYQT=%QT_ROOT%\bin\windeployqt6.exe
if not exist "%WINDEPLOYQT%" set WINDEPLOYQT=%QT_ROOT%\bin\windeployqt.exe
if not exist "%WINDEPLOYQT%" (
    echo ERROR: windeployqt not found at %QT_ROOT%\bin\
    echo        Set QT_ROOT or pass it as the second argument.
    exit /b 1
)

if not exist "%DEPLOY_DIR%" mkdir "%DEPLOY_DIR%"

:: Copy the executable first
copy /Y "%BUILD_DIR%\%CONFIG%\adgui.exe" "%DEPLOY_DIR%\" >nul
if errorlevel 1 copy /Y "%BUILD_DIR%\adgui.exe" "%DEPLOY_DIR%\" >nul

:: Copy libavrdude.dll if present (adjust path to match your build)
set AVRDUDE_DLL=%BUILD_DIR%\libavrdude.dll
if exist "%AVRDUDE_DLL%" (
    echo   Copying %AVRDUDE_DLL%...
    copy /Y "%AVRDUDE_DLL%" "%DEPLOY_DIR%\" >nul
)

:: Run windeployqt
"%WINDEPLOYQT%" ^
    --dir "%DEPLOY_DIR%" ^
    --no-system-d3d-compiler ^
    --no-opengl-sw ^
    --no-virtualkeyboard ^
    --no-quick-import ^
    --translations en ^
    "%DEPLOY_DIR%\adgui.exe"
if errorlevel 1 ( echo ERROR: windeployqt failed & exit /b 1 )

echo   Deploy directory contents:
dir /b "%DEPLOY_DIR%"

:: ── Step 4: Optional – copy avrdude.conf ────────────────────────────────────
echo.
echo [4/5] Checking for avrdude.conf...
set AVRCONF=%SCRIPT_DIR%..\avrdude.conf
if exist "%AVRCONF%" (
    copy /Y "%AVRCONF%" "%DEPLOY_DIR%\" >nul
    echo   Copied avrdude.conf to deploy dir.
) else (
    echo   avrdude.conf not found ^(optional^) – skipping.
)

:: ── Step 5: Inno Setup ──────────────────────────────────────────────────────
echo.
echo [5/5] Building installer with Inno Setup...
if not exist "%ISCC%" (
    echo WARNING: iscc.exe not found.
    echo          Install Inno Setup 6 from https://jrsoftware.org/isinfo.php
    echo          or add iscc.exe to PATH, then re-run step 5 manually:
    echo            iscc "%SCRIPT_DIR%windows.iss"
    goto :done
)

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

"%ISCC%" /O"%DIST_DIR%" "%SCRIPT_DIR%windows.iss"
if errorlevel 1 ( echo ERROR: Inno Setup compile failed & exit /b 1 )

:done
echo.
echo  ============================================================
echo   Done!
echo   Installer: %DIST_DIR%\AVRDUDE-GUI-*-win64-setup.exe
echo  ============================================================
echo.
endlocal
