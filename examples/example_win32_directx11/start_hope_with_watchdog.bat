@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process cmd -ArgumentList '/c \"\"%~f0\"\"' -Verb RunAs"
    exit /b
)
echo [*] Starting BackgroundHost with Background Watchdog (Administrator)...
start "" "%~dp0Release\watchdog.exe"
echo [SUCCESS] Watchdog is running in the background monitoring BackgroundHost.exe.
echo To stop it at any time, run stop_watchdog.bat.
timeout /t 2 >nul
