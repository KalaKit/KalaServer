@echo off
setlocal enabledelayedexpansion

set PERL=perl
set NASM=nasm
set TARGET=VC-WIN64A
set CONFIG_FLAGS=no-shared no-tests no-asm no-ui no-dso no-engine no-legacy no-camellia no-idea no-md2 no-md4 no-rc2 no-rc4 no-rc5 no-sm2 no-sm3 no-sm4 no-whirlpool no-rmd160 no-comp no-hw no-ssl3 no-ssl3-method no-weak-ssl-ciphers no-apps

set "PROJECT_ROOT=%~dp0"
set "BUILD_ROOT=%PROJECT_ROOT%\build"
set "SRC_ROOT=%PROJECT_ROOT%"

if exist %BUILD_ROOT% (
    rd /s /q %BUILD_ROOT%
)
mkdir %BUILD_ROOT%

cd /d "%SRC_ROOT%"

:: Ensure Visual Studio environment is set up correctly
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" ||(
    echo [ERROR] Failed to set up Visual Studio environment.
	pause
    exit /b 1
)

:: Build and install in Release mode
echo Configuring OpenSSL [RELEASE]
%PERL% Configure %TARGET% %CONFIG-FLAGS% --prefix="%BUILD_ROOT%\Release" --openssldir="%BUILD_ROOT%\Release\ssl"
if errorlevel 1 (
    echo [ERROR] Failed to configure OpenSSL in release!
	pause
    exit /b 1
)

echo Building OpenSSL [RELEASE]
nmake clean
nmake
if errorlevel 1 (
	echo [ERROR] OpenSSL build failed for release!
	pause
	exit /b 1
)

nmake install
if errorlevel 1 (
	echo [ERROR] OpenSSL install failed for release!
	pause
	exit /b 1
)

:: Build and install in Debug mode
echo Configuring OpenSSL [DEBUG]
%PERL% Configure %TARGET% %CONFIG-FLAGS% --prefix="%BUILD_ROOT%\Debug" --openssldir="%BUILD_ROOT%\Debug\ssl"
if errorlevel 1 (
    echo [ERROR] Failed to configure OpenSSL in debug!
	pause
    exit /b 1
)

echo Building OpenSSL [DEBUG]
nmake clean
nmake
if errorlevel 1 (
	echo [ERROR] OpenSSL build failed for debug!
	pause
	exit /b 1
)

nmake install
if errorlevel 1 (
	echo [ERROR] OpenSSL install failed for debug!
	pause
	exit /b 1
)

echo Finished building OpenSSL in release and debug mode!
pause
exit /b 0