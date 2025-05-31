@echo off

:: Check if running as administrator
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo This script must be run as administrator.
    pause
    exit /b 1
)

set "PROJECT_ROOT=%~dp0"

set "TARGET_FOLDER=%PROJECT_ROOT%build-release\example"
set "TARGET_EXE=%TARGET_FOLDER%\KalaServer_example.exe"

if not exist "%TARGET_FOLDER%" (
	echo [ERROR] Did not find KalaServer build folder from '%TARGET_FOLDER%' so server can not be started!
	pause
	exit /b 1
)
echo [SUCCESS] Found KalaServer release build folder at '%TARGET_FOLDER%'!
cd "%TARGET_FOLDER%"

if not exist "%TARGET_EXE%" (
	echo [ERROR] Did not find KalaServer exe from '%TARGET_EXE%' so server can not be started!
	pause
	exit /b 1
)
echo [SUCCESS] Found KalaServer exe at '%TARGET_EXE%'! Starting server...

:: Start KalaServer
cmd /c "%TARGET_EXE%"

echo [SUCCESS] KalaServer started successfully!
pause
exit /b 0