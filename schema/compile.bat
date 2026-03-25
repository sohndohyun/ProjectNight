@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "ROOT_DIR=%%~fI"
set "OUTPUT_DIR=%ROOT_DIR%\NightCommon\NightProtocol"

set "FLATC="
for %%F in (
    "%ROOT_DIR%\build\vcpkg_installed\x64-windows\tools\flatbuffers\flatc.exe"
    "%ROOT_DIR%\out\build\x64-debug\vcpkg_installed\x64-windows\tools\flatbuffers\flatc.exe"
    "%ROOT_DIR%\out\build\x64-release\vcpkg_installed\x64-windows\tools\flatbuffers\flatc.exe"
    "%ROOT_DIR%\vcpkg_installed\x64-windows\tools\flatbuffers\flatc.exe"
) do (
    if not defined FLATC if exist "%%~F" set "FLATC=%%~F"
)

if not defined FLATC (
    for /f "delims=" %%F in ('dir /b /s "%ROOT_DIR%\out\build\*\vcpkg_installed\x64-windows\tools\flatbuffers\flatc.exe" 2^>nul') do (
        if not defined FLATC set "FLATC=%%~F"
    )
)

if not defined FLATC (
    where flatc >nul 2>&1
    if %errorlevel% equ 0 (
        set "FLATC=flatc"
    ) else (
        echo [ERROR] flatc.exe not found.
        echo         Build the project first or add flatc to PATH.
        pause
        exit /b 1
    )
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

set "HAS_SCHEMA=0"
set "FAIL=0"

echo Using flatc: %FLATC%
for %%S in ("%SCRIPT_DIR%*.fbs") do (
    set "HAS_SCHEMA=1"
    echo Compiling: %%~nxS
    "%FLATC%" --cpp --scoped-enums -o "%OUTPUT_DIR%" "%%~fS"
    if errorlevel 1 set "FAIL=1"
)

if "%HAS_SCHEMA%" equ "0" (
    echo [ERROR] No .fbs files found in %SCRIPT_DIR%
    pause
    exit /b 1
)

if "%FAIL%" equ "0" (
    echo Schema compilation succeeded.
) else (
    echo [ERROR] Schema compilation failed.
    pause
    exit /b 1
)

pause
exit /b 0
