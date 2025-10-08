These OpenSSL binaries and headers were built with the batch script 'build-all.bat'.

To build OpenSSL yourself you must follow these steps:
1) Download Visual Studio 2022 or visual Studio Build Tools (you must have cl.exe somewhere)
2) Download and extract NASM
3) Download and install Strawberry Perl
4) Download and extract OpenSSL
5) Place both into a folder you trust and know is safe to use and won't be overwritten
6) Add the path of nasm to the environment user/system paths 
7) Run 'build-all.bat' from the same folder where you extracted OpenSSL to and wait for it to finish (it will take over 10 minutes)