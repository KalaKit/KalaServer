@echo off

set "PROJECT_ROOT=%~dp0"
cd "%PROJECT_ROOT%"

set "CONTENT_ORIGIN=%PROJECT_ROOT%\example\content"
set "CONTENT_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\content"
set "CONTENT_RELEASE_TARGET=%PROJECT_ROOT%\build-release\example\content"

set "CLOUDFLARED_ORIGIN=%PROJECT_ROOT%\example\cloudflared.exe"
set "CLOUDFLARED_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\cloudflared.exe"
set "CLOUDFLARED_RELEASE_TARGET=%PROJECT_ROOT%\build-release\example\cloudflared.exe"

set "EXT_ORIGIN=%PROJECT_ROOT%\example\whitelisted-extensions.txt"
set "EXT_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\whitelisted-extensions.txt"
set "EXT_RELEASE_TARGET=%PROJECT_ROOT%\build-release\example\whitelisted-extensions.txt"

set "WIP_ORIGIN=%PROJECT_ROOT%\example\whitelisted-ips.txt"
set "WIP_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\whitelisted-ips.txt"
set "WIP_RELEASE_TARGET=%PROJECT_ROOT%\build-release\example\whitelisted-ips.txt"

set "BIP_ORIGIN=%PROJECT_ROOT%\example\banned-ips.txt"
set "BIP_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\banned-ips.txt"
set "BIP_RELEASE_TARGET=%PROJECT_ROOT%\build-release\example\banned-ips.txt"

set "BKW_ORIGIN=%PROJECT_ROOT%\example\blacklisted-keywords.txt"
set "BKW_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\blacklisted-keywords.txt"
set "BKW_RELEASE_TARGET=%PROJECT_ROOT%\build-release\example\blacklisted-keywords.txt"

if not exist "%CONTENT_ORIGIN%" (
	echo Failed to find content folder!
	pause
	exit /b 1
)

if not exist "%CLOUDFLARED_ORIGIN%" (
	echo Failed to find cloudflared.exe!
	pause
	exit /b 1
)

set "BUILD_DEBUG=%PROJECT_ROOT%build-debug"
if exist "%BUILD_DEBUG%" (
	rmdir /s /q "%BUILD_DEBUG%"
	echo Deleted old debug build folder.
)

set "BUILD_RELEASE=%PROJECT_ROOT%build-release"
if exist "%BUILD_RELEASE%" (
	rmdir /s /q "%BUILD_RELEASE%"
	echo Deleted old release build folder.
)

echo.
echo ========================================
echo.
echo Starting "Build debug"
echo.
echo ========================================
echo.

cmd /c "build_debug.bat"
if errorlevel 1 (
    echo [ERROR] Failed to configure...
    pause
    exit /b 1
)

echo.
echo ========================================
echo.
echo Starting "Build release"
echo.
echo ========================================
echo.

cmd /c "build_release.bat"
if errorlevel 1 (
    echo [ERROR] Failed to configure...
    pause
    exit /b 1
)

echo.
echo ========================================
echo.
echo Copying server files
echo.
echo ========================================
echo.

:: Copy content folder
xcopy "%CONTENT_ORIGIN%" "%CONTENT_DEBUG_TARGET%\" /E /I /Y >nul
xcopy "%CONTENT_ORIGIN%" "%CONTENT_RELEASE_TARGET%\" /E /I /Y >nul
echo Copied server content files to targets.

:: Copy cloudflared
copy /Y "%CLOUDFLARED_ORIGIN%" "%CLOUDFLARED_DEBUG_TARGET%"
copy /Y "%CLOUDFLARED_ORIGIN%" "%CLOUDFLARED_RELEASE_TARGET%"
echo Copied cloudflared.exe to targets.

:: Copy whitelisted extensions
copy /Y "%EXT_ORIGIN%" "%EXT_DEBUG_TARGET%"
copy /Y "%EXT_ORIGIN%" "%EXT_RELEASE_TARGET%"
echo Copied whitelisted-extensions.txt to targets.

:: Copy whitelisted ips
copy /Y "%WIP_ORIGIN%" "%WIP_DEBUG_TARGET%"
copy /Y "%WIP_ORIGIN%" "%WIP_RELEASE_TARGET%"
echo Copied whitelisted-ips.txt to targets.

:: Copy banned ips
copy /Y "%BIP_ORIGIN%" "%BIP_DEBUG_TARGET%"
copy /Y "%BIP_ORIGIN%" "%BIP_RELEASE_TARGET%"
echo Copied banned-ips.txt to targets.

:: Copy blacklisted keywords
copy /Y "%BKW_ORIGIN%" "%BKW_DEBUG_TARGET%"
copy /Y "%BKW_ORIGIN%" "%BKW_RELEASE_TARGET%"
echo Copied blacklisted-keywords.txt to targets.

echo.
echo ========================================
echo.
echo Finished building in debug and release mode!

pause
exit /b 0