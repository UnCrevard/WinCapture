@echo off
setlocal

set "p=WinCapture.exe"

taskkill /f /im %p%

ren "%p%" "backup_%date%.exe"

gcc -O2 -s -municode -mwindows -o build\%p% src\main.c -lmfplat -lmfreadwrite -lmfuuid -lwindowscodecs -lole32 -loleaut32 -luuid -lshlwapi -lshell32 -lgdi32 -luser32 -lwinmm -ldwmapi -lcomdlg32 -lcomctl32 -ldxgi -ld3d11

start build\%p%