#include "std_include.hpp"
#include "dll_loading.hpp"

#ifdef _WIN32

#include <Windows.h>
#include <TlHelp32.h>
#include <filesystem>
#include <thread>
#include <atomic>

namespace fs = std::filesystem;

namespace {
    std::thread cleanupThread;
    std::atomic<bool> cleanupThreadRunning(false);
    std::string gameDirectoryForCleanup;
    DWORD gameProcessId = 0;
}

namespace DllLoading {
    bool isGameRunning() {
        if (gameProcessId != 0) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, gameProcessId);
            if (hProcess != NULL) {
                DWORD exitCode;
                if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                    CloseHandle(hProcess);
                    return true;
                }
                CloseHandle(hProcess);
                return false;
            }
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W processEntry;
        processEntry.dwSize = sizeof(processEntry);

        if (!Process32FirstW(snapshot, &processEntry)) {
            CloseHandle(snapshot);
            return false;
        }

        bool found = false;
        do {
            if (_wcsicmp(processEntry.szExeFile, L"BlackOps4.exe") == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &processEntry));

        CloseHandle(snapshot);
        return found;
    }

    void cleanupDllsAfterGame() {
        cleanupThreadRunning = true;
        std::cout << "Starting DLL cleanup monitor thread..." << std::endl;
        std::cout << "Will clean up DLLs in directory: " << gameDirectoryForCleanup << std::endl;

        while (isGameRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        std::cout << "Game has terminated. Cleaning up DLLs..." << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(3));

        if (gameDirectoryForCleanup.empty()) {
            std::cout << "Error: Game directory path is empty" << std::endl;
            cleanupThreadRunning = false;
            return;
        }

        fs::path dllPath = fs::path(gameDirectoryForCleanup) / "XInput9_1_0.dll";

        std::cout << "Attempting to delete DLL at: " << dllPath.string() << std::endl;

        try {
            if (fs::exists(dllPath)) {
                std::cout << "DLL found, deleting: " << dllPath.string() << std::endl;

                for (int i = 0; i < 5; i++) {
                    try {
                        if (fs::remove(dllPath)) {
                            std::cout << "Successfully deleted DLL" << std::endl;
                            break;
                        } else {
                            std::cout << "Failed to delete DLL, attempt " << (i+1) << std::endl;
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                    } catch (const std::exception& e) {
                        std::cout << "Error deleting DLL (attempt " << (i+1) << "): " << e.what() << std::endl;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
            } else {
                std::cout << "DLL not found at: " << dllPath.string() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Error accessing DLL path: " << e.what() << std::endl;
        }

        cleanupThreadRunning = false;
    }

    bool launchGame(const std::string& gameExePath, bool isOnline) {
        std::cout << "Launching game: " << gameExePath << (isOnline ? " -multiplayer" : " -zombies") << std::endl;

        fs::path gameDir = fs::path(gameExePath).parent_path();
        std::string gameDirStr = gameDir.string();

        gameDirectoryForCleanup = gameDirStr;

        std::string gameFlags = isOnline ? " -multiplayer" : " -zombies";

        SHELLEXECUTEINFOA shExInfo = {0};
        shExInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
        shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        shExInfo.hwnd = NULL;
        shExInfo.lpVerb = "open";
        shExInfo.lpFile = gameExePath.c_str();
        shExInfo.lpParameters = gameFlags.c_str();
        shExInfo.lpDirectory = gameDirStr.c_str();
        shExInfo.nShow = SW_SHOW;

        std::cout << "Launching from directory: " << gameDirStr << std::endl;

        if (!ShellExecuteExA(&shExInfo)) {
            DWORD error = GetLastError();
            std::cout << "ShellExecuteEx failed with error: " << error << std::endl;
            return false;
        }

        if (shExInfo.hProcess) {
            CloseHandle(shExInfo.hProcess);
        }

        if (cleanupThreadRunning && cleanupThread.joinable()) {
            cleanupThread.join();
        }

        cleanupThread = std::thread(cleanupDllsAfterGame);
        cleanupThread.detach();

        std::cout << "Game launched successfully!" << std::endl;
        return true;
    }
}

#endif // _WIN32
