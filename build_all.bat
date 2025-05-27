@echo off

set "PROJECT_ROOT=%~dp0"
cd "%PROJECT_ROOT%"

set "PAGES_ORIGIN=%PROJECT_ROOT%\example\pages"
set "IMAGES_ORIGIN=%PROJECT_ROOT%\example\images"
set "VIDEOS_ORIGIN=%PROJECT_ROOT%\example\videos"

set "PAGES_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\pages"
set "IMAGES_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\images"
set "VIDEOS_DEBUG_TARGET=%PROJECT_ROOT%\build-debug\example\videos"

set "PAGES_RELEASE_TARGET=%PROJECT_ROOT%\build-release\example\pages"
set "IMAGES_RELEASE_TARGET=%PROJECT_ROOT%\build-release\example\images"
set "VIDEOS_RELEASE_TARGET=%PROJECT_ROOT%\build-release\example\videos"

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

mkdir "%PAGES_DEBUG_TARGET%"
mkdir "%IMAGES_DEBUG_TARGET%"
mkdir "%VIDEOS_DEBUG_TARGET%"

mkdir "%PAGES_RELEASE_TARGET%"
mkdir "%IMAGES_RELEASE_TARGET%"
mkdir "%VIDEOS_RELEASE_TARGET%"

xcopy "%PAGES_ORIGIN%" "%PAGES_DEBUG_TARGET%\" /E /I /Y >nul
echo Copied files to pages debug target.
xcopy "%IMAGES_ORIGIN%" "%IMAGES_DEBUG_TARGET%\" /E /I /Y >nul
echo Copied files to images debug target.
xcopy "%VIDEOS_ORIGIN%" "%VIDEOS_DEBUG_TARGET%\" /E /I /Y >nul
echo Copied files to videos debug target.

xcopy "%PAGES_ORIGIN%" "%PAGES_RELEASE_TARGET%\" /E /I /Y >nul
echo Copied files to pages release target.
xcopy "%IMAGES_ORIGIN%" "%IMAGES_RELEASE_TARGET%\" /E /I /Y >nul
echo Copied files to images release target.
xcopy "%VIDEOS_ORIGIN%" "%VIDEOS_RELEASE_TARGET%\" /E /I /Y >nul
echo Copied files to videos release target.

echo.
echo ========================================
echo.
echo Finished building in debug and release mode!

pause
exit /b 0