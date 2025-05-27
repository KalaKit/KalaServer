@echo off

set "PROJECT_ROOT=%~dp0"
cd "%PROJECT_ROOT%"

set "VS_MAIN=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set "VS_SIDE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if exist "%VS_MAIN%" (
    call "%VS_MAIN%"
) else if exist "%VS_SIDE%" (
    call "%VS_SIDE%"
) else (
    echo [ERROR] Could not find vcvars64.bat from either Visual Studio Community or Build Tools. Please download Visual Studio or Visual Studio Build Tools.
    pause
    exit /b 1
)

:: Configure in release
cmake --preset windows-release
if errorlevel 1 (
    echo [ERROR] Failed to configure...
    pause
    exit /b 1
)

:: Build in release
cmake --build --preset windows-release
if errorlevel 1 (
    echo [ERROR] Failed to build...
    pause
    exit /b 1
)

pause
exit /b 0