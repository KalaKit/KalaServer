@echo off

set "PROJECT_ROOT=%~dp0"
cd "%PROJECT_ROOT%"

set "STATIC_ORIGIN=%PROJECT_ROOT%\example\static"
set "STATIC_TARGET=%PROJECT_ROOT%\build-debug\example\static"

:: Ensure Visual Studio environment is set up correctly
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" ||(
    echo [ERROR] Failed to set up Visual Studio environment.
	pause
    exit /b 1
)

:: Configure in debug
cmake --preset windows-debug
if errorlevel 1 (
    echo [ERROR] Failed to configure...
    pause
    exit /b 1
)

:: Build in debug
cmake --build --preset windows-debug
if errorlevel 1 (
    echo [ERROR] Failed to build...
    pause
    exit /b 1
)

:: Copy static folder content to server folder
if exist "%STATIC_TARGET%" (
	rmdir /s /q "%STATIC_TARGET%"
	echo Deleted old static folder folder.
	echo.
)

mkdir "%STATIC_TARGET%"
xcopy "%STATIC_ORIGIN%" "%STATIC_TARGET%\" /E /I /Y >nul

pause
exit /b 0