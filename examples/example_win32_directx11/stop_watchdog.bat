@echo off
echo [*] Stopping Watchdog and BackgroundHost processes...
taskkill /F /IM watchdog.exe /T >nul 2>&1
taskkill /F /IM BackgroundHost.exe /T >nul 2>&1
taskkill /F /IM hope.exe /T >nul 2>&1
powershell -NoProfile -Command "Get-Process -Name 'watchdog', 'BackgroundHost', 'hope' -ErrorAction SilentlyContinue | Stop-Process -Force" >nul 2>&1
echo [SUCCESS] Watchdog and BackgroundHost have been stopped completely.
ping -n 3 127.0.0.1 >nul
