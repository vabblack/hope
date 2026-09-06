@echo off
setlocal enabledelayedexpansion

echo ==========================================================
echo    Building Watchdog and Signing Binaries
echo ==========================================================

:: 1. Ensure Release directory exists
if not exist "Release" mkdir Release

:: 2. Set up Visual Studio Build Environment if cl is not in PATH
where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo [*] Setting up MSVC environment...
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
    ) else if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
    ) else (
        echo [ERROR] Could not find vcvars64.bat. Please run from a Visual Studio Developer Command Prompt.
        exit /b 1
    )
)

:: 3. Compile watchdog.cpp as a Windows subsystem binary (No console window, AsInvoker)
echo [*] Compiling watchdog.cpp into Release\watchdog.exe...
cl /nologo /O2 /MD /utf-8 /D UNICODE /D _UNICODE watchdog.cpp /FeRelease\watchdog.exe /FoRelease\ /link /SUBSYSTEM:WINDOWS /MANIFESTUAC:"level='asInvoker' uiAccess='false'" Shell32.lib User32.lib Kernel32.lib
if %errorlevel% neq 0 (
    echo [ERROR] Compilation of watchdog.cpp failed!
    exit /b %errorlevel%
)

echo [SUCCESS] Release\watchdog.exe compiled successfully!

:: 4. Run PowerShell signing script to sign both watchdog.exe and hope.exe
echo [*] Invoking PowerShell Authenticode signing pipeline...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\sign_binaries.ps1
if %errorlevel% neq 0 (
    echo [ERROR] Signing script encountered an error!
    exit /b %errorlevel%
)

echo ==========================================================
echo [SUCCESS] Watchdog build and binary signing complete!
echo ==========================================================
