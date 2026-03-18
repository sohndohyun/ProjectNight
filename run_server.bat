@echo off
chcp 65001 >nul

echo [ProjectNight] NightServer 빌드 및 실행
echo.

:: VS 개발 환경이 이미 설정되어 있으면 건너뛰기
where cl.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo [환경] 이미 설정됨 - 건너뜀
    goto :build
)

echo [환경] Visual Studio 환경 설정 중...
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set VS_PATH=%%i
if not defined VS_PATH (
    echo [에러] Visual Studio를 찾을 수 없습니다.
    pause
    exit /b 1
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

:build
pushd "%~dp0NightServer"

if not exist "out\build\x64-debug\build.ninja" (
    echo [빌드] CMake Configure...
    cmake --preset x64-debug
    if %errorlevel% neq 0 ( echo [에러] Configure 실패 & popd & pause & exit /b 1 )
) else (
    echo [빌드] Configure 스킵 (이미 구성됨)
)

echo [빌드] CMake Build...
cmake --build out/build/x64-debug
if %errorlevel% neq 0 ( echo [에러] Build 실패 & popd & pause & exit /b 1 )
popd

:: 실행
echo.
echo ========================================
"%~dp0NightServer\out\build\x64-debug\NightServer.exe"
pause
