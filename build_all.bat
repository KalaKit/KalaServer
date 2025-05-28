@echo off

set "PROJECT_ROOT=%~dp0"
cd "%PROJECT_ROOT%"

set "CONTENT_ORIGIN=%PROJECT_ROOT%\example\content"
set "CONTENT_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\content"
set "CONTENT_RELEASE_TARGET=%PROJECT_ROOT%\build-release\example\content"

set "CLOUDFLARED_ORIGIN=%PROJECT_ROOT%\example\cloudflared.exe"
set "CLOUDFLARED_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\cloudflared.exe"
set "CLOUDFLARED_RELEASE_TARGET=%PROJECT_ROOT%\\build-release\example\cloudflared.exe"

if not exist "%CLOUDFLARED_ORIGIN%" (
	echo Failed to find cloudflared.exe! Please place it to "%PROJECT_ROOT%example\cloudflared.exe".
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

:: Delete old targets
if exist "%PAGES_DEBUG_TARGET%" rmdir /s /q "%PAGES_DEBUG_TARGET%"
if exist "%IMAGES_DEBUG_TARGET%" rmdir /s /q "%IMAGES_DEBUG_TARGET%"
if exist "%VIDEOS_DEBUG_TARGET%" rmdir /s /q "%VIDEOS_DEBUG_TARGET%"

if exist "%PAGES_RELEASE_TARGET%" rmdir /s /q "%PAGES_RELEASE_TARGET%"
if exist "%IMAGES_RELEASE_TARGET%" rmdir /s /q "%IMAGES_RELEASE_TARGET%"
if exist "%VIDEOS_RELEASE_TARGET%" rmdir /s /q "%VIDEOS_RELEASE_TARGET%"

xcopy "%CONTENT_ORIGIN%" "%CONTENT_DEBUG_TARGET%\" /E /I /Y >nul
echo Copied server content files to debug target.
xcopy "%CONTENT_ORIGIN%" "%CONTENT_RELEASE_TARGET%\" /E /I /Y >nul
echo Copied server content files to release target.

copy /Y "%CLOUDFLARED_ORIGIN%" "%CLOUDFLARED_DEBUG_TARGET%"
echo Copied cloudflared to debug target.
copy /Y "%CLOUDFLARED_ORIGIN%" "%CLOUDFLARED_RELEASE_TARGET%"
echo Copied cloudflared to release target.

echo.
echo ========================================
echo.
echo Finished building in debug and release mode!

pause
exit /b 0