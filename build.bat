@echo off
setlocal EnableDelayedExpansion

set "p=WinCapture.exe"
set "backup_dir=backup"

if not exist "%backup_dir%" mkdir "%backup_dir%"

taskkill /f /im %p% >nul 2>&1

mingw32-make.exe
if %ERRORLEVEL% neq 0 (
    echo [BUILD ERROR] La compilation a echoue.
    exit /b %ERRORLEVEL%
)

:: Horodatage format YYYY-MM-DD_HH-mm-ss
set "d=%date: =%"
set "t=%time: =0%"
set "t=%t::=-%"
set "t=%t:,=.%"
set "t=%t:~0,8%"
set "backup_file=%backup_dir%\WinCapture_%d%_%t%.exe"

copy /y "%p%" "%backup_file%" >nul
if %ERRORLEVEL% equ 0 (
    echo [BACKUP] Copie creee : %backup_file%
) else (
    echo [WARNING] Impossible de copier %p% vers %backup_file%
)

start "" "%p%"
