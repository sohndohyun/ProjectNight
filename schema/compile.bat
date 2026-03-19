@echo off
setlocal

set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%..
set OUTPUT_DIR=%ROOT_DIR%\NightCommon\NightProtocol

:: vcpkg tools에서 flatc 탐색
set FLATC=
set VCPKG_FLATC=%ROOT_DIR%\build\vcpkg_installed\x64-windows\tools\flatbuffers\flatc.exe
if exist "%VCPKG_FLATC%" (
    set "FLATC=%VCPKG_FLATC%"
)

if not defined FLATC (
    where flatc >nul 2>&1
    if %errorlevel% equ 0 (
        set "FLATC=flatc"
    ) else (
        echo [ERROR] flatc.exe not found.
        echo         Build the project first or add flatc to PATH.
        exit /b 1
    )
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo Using flatc: %FLATC%

set FAIL=0
for %%s in ("%SCRIPT_DIR%*.fbs") do (
    echo Compiling: %%~nxs
    "%FLATC%" --cpp --scoped-enums -o "%OUTPUT_DIR%" "%%s"
    if errorlevel 1 set FAIL=1
)

if %FAIL% equ 0 (
    echo Schema compilation succeeded.
) else (
    echo [ERROR] Schema compilation failed.
    exit /b 1
)
