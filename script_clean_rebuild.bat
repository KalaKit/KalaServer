@echo off

set "PROJECT_ROOT=%~dp0"
cd "%PROJECT_ROOT%"

set "BUILD_DIR=%PROJECT_ROOT%build-debug"
if exist "%BUILD_DIR%" (
	rmdir /s /q "%BUILD_DIR%"
	echo Deleted old build_debug folder.
	echo.
)

cmd /c "script_regular_build.bat"