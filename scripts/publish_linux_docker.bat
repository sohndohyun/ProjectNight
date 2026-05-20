@echo off
setlocal
chcp 65001 >nul

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "ROOT_DIR=%%~fI"
set "IMAGE=ubuntu:24.04"

echo [ProjectNight] Docker Linux Release 빌드
echo.

where docker.exe >nul 2>&1
if errorlevel 1 (
    echo [에러] docker를 찾을 수 없습니다.
    echo        Docker Desktop과 Linux container 환경이 필요합니다.
    pause
    exit /b 1
)

pushd "%ROOT_DIR%"

if not exist "publish" mkdir "publish"
if exist "publish\linux-release" (
    echo [정리] 기존 publish\linux-release 삭제
    rmdir /s /q "publish\linux-release"
)

echo [Docker] 컨테이너에서 의존성 설치, Configure, Build, Publish를 수행합니다...
docker run --rm ^
    -v "%ROOT_DIR%:/workspace" ^
    -w /workspace ^
    "%IMAGE%" ^
    bash -lc "set -euo pipefail && export DEBIAN_FRONTEND=noninteractive && apt-get update && apt-get install -y build-essential cmake ninja-build pkg-config curl zip unzip tar git ca-certificates autoconf automake libtool xorg-dev libgtk-3-dev libglu1-mesa-dev libnotify-dev libsecret-1-dev && ./vcpkg/bootstrap-vcpkg.sh -disableMetrics && cmake -S . -B out/build/linux-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/workspace/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux && cmake --build out/build/linux-release --config Release && mkdir -p publish/linux-release/NightServer publish/linux-release/NightClient && cp -f out/build/linux-release/NightServer/NightServer publish/linux-release/NightServer/ && cp -f out/build/linux-release/NightClient/NightClient publish/linux-release/NightClient/"
if errorlevel 1 (
    echo [에러] Docker Linux Release 빌드 실패
    popd
    pause
    exit /b 1
)

echo.
echo [완료] publish\linux-release
popd
pause
