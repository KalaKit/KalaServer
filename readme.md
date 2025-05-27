# KalaServer

[![License](https://img.shields.io/badge/license-Zlib-blue)](LICENSE.md)
![Platform](https://img.shields.io/badge/platform-Windows-brightgreen)
![Development Stage](https://img.shields.io/badge/development-Alpha-yellow)

![Logo](logo.png)

KalaServer is a lightweight C++ 20 library for Windows that allows you to make an HTTP server locally for websites using Windows sockets.

Future plans include support for file hosting, file download/upload, media playback, secure account systems with authorization and easy game server creation. Track progress on the [trello board](https://trello.com/b/Xrf2qRDD/kalaserver).

---

# Prerequisites (when compiling from source code)

- Visual Studio 2022 (with C++ CMake tools and Windows 10 or 11 SDK)
- Ninja and CMake 3.30.3 or newer (or extract Windows_prerequsites.7z and run setup.bat)

To compile from source code simply run 'build_all.bat' or 'build_debug.bat' or 'build_release.bat' depending on your preferences.

---

# How to use

Compile the project from source using the existing CMakeLists.txt at root by running 'build_all.bat' and then open 'KalaServer_example.exe' inside 'build-release/example' or 'build-debug/example' to start the example server made with KalaServer.

## How to get access locally

- allow port '8080' through firewall
	- add inbound rule
	- choose port
	- choose tcp
	- set specific local port to 8080
	- allow connection
	- profile: private
	- name: KalaServer
- run the server (which listens on port 8080)
- open your browser and go to 'http://localhost:8080'

## How to get access locally with a custom domain

- open 'C:\Windows\System32\drivers\etc\hosts'
- add '127.0.0.1    yourdomain.com' at the bottom
- allow port '8080' through firewall
	- add inbound rule
	- choose port
	- choose tcp
	- set specific local port to 8080
	- allow connection
	- profile: All
	- name: KalaServer
- run the server (which listens on port 8080)
- open your browser and go to 'http://yourdomain.com:8080'

## How to get access globally

- allow port '80' through firewall
	- add inbound rule
	- choose port
	- choose tcp
	- set specific local port to 80
	- allow connection
	- profile: All
	- name: KalaServer
- log in to your router admin page (usually 192.168.1.1)
- go to the port forwarding section 
	- port: 80
	- protocol: tcp
	- internal ip: your ipv4
	- internal port: 80
	- name: KalaServer
- add a root domain record to cloudflare (or other domain provider)
	- go to your cloudflare domain
	- go to dns
	- click add record
	- Type: A
	- Name: @
	- IPv4 address: your public ipv4 (not the same from cmd ipconfig command)
	- Proxy status: off
- add a cname record to cloudflare (or other domain provider)
	- go to your cloudflare domain
	- go to dns
	- click add record
	- Type: CNAME
	- Name: www
	- Target: your domain + extension (example - thekalakit.com)
	- Proxy status: off
- run the server as admin (which listens on port 80)
- open your browser and go to 'http://yourdomain.com'

https://accounts.hetzner.com/login