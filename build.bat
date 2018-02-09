@echo off


call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64

cl -Zi -Fe: bin\ -Fo: bin\inter\ -FR: bin\inter\ code\win32_handmade.cpp -Fd: bin\inter\  user32.lib Gdi32.lib
popd


pause