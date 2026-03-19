@echo off
chcp 65001 >nul

:: 사용법: run_client.bat [클라이언트 수]
:: 예시: run_client.bat 3    → 클라이언트 3개 실행
::       run_client.bat      → 클라이언트 1개 실행

set CLIENT_COUNT=%1
if "%CLIENT_COUNT%"=="" set CLIENT_COUNT=1

echo [ProjectNight] NightClient 빌드 및 실행 (x%CLIENT_COUNT%)
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
pushd "%~dp0"

if not exist "out\build\x64-debug\build.ninja" (
    echo [빌드] CMake Configure...
    cmake --preset x64-debug
    if %errorlevel% neq 0 ( echo [에러] Configure 실패 & popd & pause & exit /b 1 )
) else (
    echo [빌드] Configure 스킵 (이미 구성됨)
)

echo [빌드] CMake Build (NightClient)...
cmake --build out/build/x64-debug --target NightClient
if %errorlevel% neq 0 ( echo [에러] Build 실패 & popd & pause & exit /b 1 )
popd

:: 실행
set EXE_PATH=%~dp0out\build\x64-debug\NightClient\NightClient.exe
echo.
echo ========================================

for /l %%i in (1,1,%CLIENT_COUNT%) do (
    echo [실행] 클라이언트 #%%i 시작
    start "NightClient #%%i" cmd /k "chcp 65001 >nul & "%EXE_PATH%""
)

echo.
echo [완료] 클라이언트 %CLIENT_COUNT%개 실행됨
pause
