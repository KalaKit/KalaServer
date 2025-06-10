# KalaServer

[![License](https://img.shields.io/badge/license-Zlib-blue)](LICENSE.md)
![Platform](https://img.shields.io/badge/platform-Windows-brightgreen)
![Development Stage](https://img.shields.io/badge/development-Alpha-yellow)

![Logo](logo.png)

KalaServer is a lightweight C++ 20 library for Windows that allows you to make web, media or any other kind of a server with very few dependencies. The library is currently in early alpha development, so a lot of changes will come, but the core goals will stay the same. Track development and future plans on the public [trello board](https://trello.com/b/Xrf2qRDD/kalaserver) board.

---
 
# OpenSSL

This library includes OpenSSL binaries and headers at [its folder](external/OpenSSL).

OpenSSL is licensed under its own terms:  
https://www.openssl.org/source/license.html

We do not own OpenSSL. Use of its components is subject to that license.


---

# Prerequisites (when compiling from source code)

- Visual Studio 2022 (with C++ CMake tools and Windows 10 or 11 SDK)
- Ninja and CMake 3.30.3 or newer (or extract Windows_prerequsites.7z and run setup.bat)

To compile from source code simply run 'build_all.bat' or 'build_debug.bat' or 'build_release.bat' depending on your preferences.

---

# How to use

Copy KalaServer.lib to your exe directory and compile your preferred way and follow the [documentation](https://thekalakit.com/pages/docs/kalaserver-setup) for more info on how to set up the code or look at the [source code](https://github.com/Lost-Empire-Entertainment/KalaKit-website) for the KalaKit website. 

Documentation for connecting via url/ip:
- [How to get access locally without a domain](docs/connect/local-access.md)
- [How to get access locally with a domain](docs/connect/local-domain-access.md)
- [How to get access globally with cloudflared](docs/connect/global-cloudflared-access.md)
- [How to get access globally via port forwarding](docs/connect/global-port-forward-access.md)