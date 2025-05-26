# KalaServer

[![License](https://img.shields.io/badge/license-Zlib-blue)](LICENSE.md)
![Platform](https://img.shields.io/badge/platform-Windows-brightgreen)
![Development Stage](https://img.shields.io/badge/development-Alpha-yellow)

![Logo](logo.png)

KalaServer is a lightweight C++ 20 library for Windows that allows you to make a server locally, this is primarily aimed for websites but you're welcome to use this for other networking related things too.

# Prerequisites (when compiling from source code)

- Visual Studio 2022 (with C++ CMake tools and Windows 10 or 11 SDK)
- Ninja and CMake 3.30.3 or newer (or extract Windows_prerequsites.7z and run setup.bat)

To compile from source code simply run 'build_windows_release.bat' or 'build_windows_debug.bat' depending on your preferences then copy and attach the dll, lib and header files with your preferred way to your program source directory.

# How to use

The example project found in the example folder shows how to set up and run KalaServer.

how to get access locally:
- local port: 8080
- local website: http://localhost:8080/
- firewall: enable private, disable public