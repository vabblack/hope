#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <algorithm>

// Helper to determine directory from a file path
std::wstring GetDirectoryFromPath(const std::wstring& path) {
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return path.substr(0, lastSlash);
    }
    return L"";
}

// Helper to check if file exists
bool FileExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

// Helper to sanitize identifier for named mutex/events
std::wstring MakeSafeIdentifier(const std::wstring& prefix, const std::wstring& targetExe) {
    std::wstring name = prefix;
    for (wchar_t ch : targetExe) {
        if (iswalnum(ch)) name += ch;
        else name += L'_';
    }
    return name;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    // 1. Determine directory of this watchdog executable
    wchar_t watchdogExePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, watchdogExePath, MAX_PATH);
    std::wstring watchdogDir = GetDirectoryFromPath(watchdogExePath);

    // 2. Parse command line
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::wstring targetExe = L"";
    std::wstring targetArgs = L"";
    DWORD restartDelayMs = 3000;
    bool exitOnClean = true;
    bool stopRequested = false;

    int argIndex = 1;
    while (argIndex < argc) {
        std::wstring arg = argv[argIndex];
        if (arg == L"--delay" && argIndex + 1 < argc) {
            restartDelayMs = (DWORD)_wtoi(argv[++argIndex]);
            if (restartDelayMs < 500) restartDelayMs = 500;
        }
        else if (arg == L"--always-restart") {
            exitOnClean = false;
        }
        else if (arg == L"--exit-on-clean") {
            exitOnClean = true;
        }
        else if (arg == L"--stop") {
            stopRequested = true;
        }
        else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            MessageBoxW(NULL,
                L"Windows Process Watchdog (Background Runner & Auto-Restarter)\n\n"
                L"Usage:\n"
                L"  watchdog.exe [options] [\"Path\\To\\AnyProcess.exe\"] [process arguments...]\n\n"
                L"Options:\n"
                L"  --delay <ms>       Delay before restart in milliseconds (default: 3000)\n"
                L"  --exit-on-clean    Stop watchdog if the target process exits cleanly (exit code 0)\n"
                L"  --stop             Signal the running watchdog for this target to gracefully stop\n"
                L"  --help             Show this help message\n\n"
                L"Default Behavior:\n"
                L"  If no target process is given, supervises hope.exe automatically.",
                L"Watchdog Help",
                MB_OK | MB_ICONINFORMATION);
            LocalFree(argv);
            return 0;
        }
        else {
            targetExe = arg;
            argIndex++;
            for (int i = argIndex; i < argc; i++) {
                if (!targetArgs.empty()) targetArgs += L" ";
                std::wstring subArg = argv[i];
                if (subArg.find(L' ') != std::wstring::npos) {
                    targetArgs += L"\"" + subArg + L"\"";
                }
                else {
                    targetArgs += subArg;
                }
            }
            break;
        }
        argIndex++;
    }

    LocalFree(argv);

    // 3. Fallback to BackgroundHost.exe (or hope.exe) if no target specified
    if (targetExe.empty()) {
        if (FileExists(watchdogDir + L"\\BackgroundHost.exe")) {
            targetExe = watchdogDir + L"\\BackgroundHost.exe";
        }
        else if (FileExists(watchdogDir + L"\\Release\\BackgroundHost.exe")) {
            targetExe = watchdogDir + L"\\Release\\BackgroundHost.exe";
        }
        else if (FileExists(watchdogDir + L"\\hope.exe")) {
            targetExe = watchdogDir + L"\\hope.exe";
        }
        else if (FileExists(watchdogDir + L"\\Release\\hope.exe")) {
            targetExe = watchdogDir + L"\\Release\\hope.exe";
        }
        else if (FileExists(watchdogDir + L"\\..\\Release\\BackgroundHost.exe")) {
            targetExe = watchdogDir + L"\\..\\Release\\BackgroundHost.exe";
        }
        else {
            targetExe = watchdogDir + L"\\BackgroundHost.exe";
        }
    }

    // Resolve relative path to absolute path
    wchar_t fullTargetPath[MAX_PATH] = { 0 };
    if (GetFullPathNameW(targetExe.c_str(), MAX_PATH, fullTargetPath, NULL) > 0) {
        targetExe = fullTargetPath;
    }

    std::wstring targetDir = GetDirectoryFromPath(targetExe);
    if (targetDir.empty()) {
        targetDir = watchdogDir;
    }

    // Identifiers for mutex and stop event
    std::wstring mutexName = MakeSafeIdentifier(L"Local\\Watchdog_Mutex_", targetExe);
    std::wstring stopEventName = MakeSafeIdentifier(L"Local\\Watchdog_Stop_", targetExe);

    // If --stop was passed, signal any existing watchdog monitoring this target
    if (stopRequested) {
        HANDLE hExistingStop = OpenEventW(EVENT_MODIFY_STATE, FALSE, stopEventName.c_str());
        if (hExistingStop) {
            SetEvent(hExistingStop);
            CloseHandle(hExistingStop);
        }
        return 0;
    }

    // Verify target executable exists before entering loop
    if (!FileExists(targetExe)) {
        std::wstring msg = L"Target executable not found:\n" + targetExe;
        MessageBoxW(NULL, msg.c_str(), L"Watchdog Error", MB_OK | MB_ICONERROR);
        return 2;
    }

    // 4. Single-instance mutex: prevent multiple watchdogs from monitoring the same target
    HANDLE hMutex = CreateMutexW(NULL, TRUE, mutexName.c_str());
    if (hMutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
        // Already running for this target
        CloseHandle(hMutex);
        return 1;
    }

    // Create the stop event (manual-reset) and ensure it starts in non-signaled state
    HANDLE hStopEvent = CreateEventW(NULL, TRUE, FALSE, stopEventName.c_str());
    if (hStopEvent) {
        ResetEvent(hStopEvent);
    }

    // 5. Main Watchdog Loop with Rapid-Exit Throttling
    int rapidExitCount = 0;

    while (true) {
        if (WaitForSingleObject(hStopEvent, 0) == WAIT_OBJECT_0) {
            break;
        }

        STARTUPINFOW si = { 0 };
        PROCESS_INFORMATION pi = { 0 };
        si.cb = sizeof(si);

        std::wstring commandLine = L"\"" + targetExe + L"\"";
        if (!targetArgs.empty()) {
            commandLine += L" " + targetArgs;
        }

        std::vector<wchar_t> cmdBuf(commandLine.begin(), commandLine.end());
        cmdBuf.push_back(L'\0');

        DWORD launchTime = GetTickCount();

        BOOL success = CreateProcessW(
            NULL,
            cmdBuf.data(),
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            targetDir.c_str(),
            &si,
            &pi
        );

        if (success && pi.dwProcessId != 0) {
            AllowSetForegroundWindow(pi.dwProcessId);
        }

        // If target requires UAC elevation (ERROR_ELEVATION_REQUIRED = 740), fallback to ShellExecuteExW
        if (!success && GetLastError() == ERROR_ELEVATION_REQUIRED) {
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpFile = targetExe.c_str();
            sei.lpParameters = targetArgs.empty() ? NULL : targetArgs.c_str();
            sei.lpDirectory = targetDir.c_str();
            sei.lpVerb = L"runas";
            sei.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&sei) && sei.hProcess != NULL) {
                pi.hProcess = sei.hProcess;
                pi.hThread = NULL;
                success = TRUE;
                AllowSetForegroundWindow(ASFW_ANY);
            }
        }

        if (!success) {
            DWORD waitRes = WaitForSingleObject(hStopEvent, restartDelayMs > 5000 ? restartDelayMs : 5000);
            if (waitRes == WAIT_OBJECT_0) break;
            continue;
        }

        // Wait for either the child process to exit OR the stop event to be signaled
        HANDLE waitHandles[2] = { pi.hProcess, hStopEvent };
        DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

        if (waitRes == WAIT_OBJECT_0 + 1) {
            // Stop event was signaled
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            break;
        }

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        // Handle desktop jump: child process spawned a new instance on another desktop
        // (e.g. SEB or alternate desktop session) and exited with code 0xDE5C
        if (exitCode == 0xDE5C) {
            Sleep(100);
            HANDLE hMap = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\Watchdog_JumpPid");
            DWORD newPid = 0;
            if (hMap) {
                DWORD* pPid = (DWORD*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, sizeof(DWORD));
                if (pPid) {
                    newPid = *pPid;
                    UnmapViewOfFile(pPid);
                }
                CloseHandle(hMap);
            }
            if (newPid != 0) {
                HANDLE hNewProc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, newPid);
                if (hNewProc != NULL) {
                    pi.hProcess = hNewProc;
                    pi.dwProcessId = newPid;
                    pi.hThread = NULL;
                    rapidExitCount = 0;
                    continue; // Seamlessly adopt child on new desktop without spawning duplicates
                }
            }
        }

        DWORD duration = GetTickCount() - launchTime;

        // If user wants clean exit to terminate watchdog
        if (exitOnClean && exitCode == 0) {
            break;
        }

        // Check if stop was signaled
        if (WaitForSingleObject(hStopEvent, 0) == WAIT_OBJECT_0) {
            break;
        }

        // Rapid-exit loop protection (e.g. apps that exit instantly or launch shims like Win11 notepad)
        if (duration < 2000) {
            rapidExitCount++;
            if (rapidExitCount >= 4) {
                // If it exited immediately 4 times in a row, back off for 15 seconds to avoid spam
                if (WaitForSingleObject(hStopEvent, 15000) == WAIT_OBJECT_0) {
                    break;
                }
                rapidExitCount = 0;
                continue;
            }
        }
        else {
            rapidExitCount = 0;
        }

        // Normal cooldown delay
        if (WaitForSingleObject(hStopEvent, restartDelayMs) == WAIT_OBJECT_0) {
            break;
        }
    }

    if (hStopEvent) CloseHandle(hStopEvent);
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return 0;
}
