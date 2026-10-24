#include "std_include.hpp"
#include "dll_loading.hpp"
#include "zip_archive.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <zlib.h>
#include <QCoreApplication>
#include <thread>
#include <atomic>

namespace fs = std::filesystem;

namespace {
    // CRC-32 for checking dlls and exe
    bool computeFileCrc32(const fs::path& filePath, uint32_t& outCrc) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            return false;
        }

        uLong crc = crc32(0L, Z_NULL, 0);
        std::vector<char> buffer(1 << 16);
        while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
            crc = crc32(crc, reinterpret_cast<const Bytef*>(buffer.data()), static_cast<uInt>(file.gcount()));
        }

        outCrc = static_cast<uint32_t>(crc);
        return true;
    }
}

namespace DllLoading {

    Result extractDlls(const std::string& gameDir, bool isOnline, bool reshadeEnabled) {
        if (gameDir.empty()) {
            return Result::InvalidGamePath;
        }

        fs::path gamePath = fs::path(gameDir);
        fs::path gameExePath = gamePath / "BlackOps4.exe";

        std::cout << "Initial game path: " << gamePath.string() << std::endl;
        std::cout << "Looking for game exe at: " << gameExePath.string() << std::endl;

        if (!fs::exists(gameExePath)) {
            fs::path parentPath = gamePath.parent_path().parent_path();
            gameExePath = parentPath / "BlackOps4.exe";

            std::cout << "Trying parent path: " << parentPath.string() << std::endl;
            std::cout << "Looking for game exe at: " << gameExePath.string() << std::endl;

            if (!fs::exists(gameExePath)) {
                std::cout << "Game exe not found at parent path either" << std::endl;
                return Result::InvalidGamePath;
            }

            gamePath = parentPath;
            std::cout << "Updated game path to: " << gamePath.string() << std::endl;
        }

        fs::path launcherDir = gamePath / "project-bo4" / "launcher";
        std::cout << "Launcher directory: " << launcherDir.string() << std::endl;

        fs::path zipPath = launcherDir / (isOnline ? "mp.zip" : "solo.zip");
        std::cout << "Looking for zip at: " << zipPath.string() << std::endl;
        std::cout << "File exists: " << (fs::exists(zipPath) ? "Yes" : "No") << std::endl;

        if (!fs::exists(zipPath)) {
            return Result::FileNotFound;
        }


        // Note: not needed anymore cause of ATE47's plugin now loading zone from project-bo4/zone

        /*
        // zone shit
        std::cout << "Copying zone folders..." << std::endl;

        // --- Copy project-bo4/zone to <gameDir>/zone ---
        try {
            fs::path sourceZone = gamePath / "project-bo4" / "zone";
            fs::path targetZone = gamePath / "zone";

            if(fs::exists(sourceZone)) {
                std::cout << "Copying zone files from: " << sourceZone.string() << " to: " << targetZone.string() << std::endl;
                fs::create_directories(targetZone); // target exists?

                for(const auto& entry : fs::directory_iterator(sourceZone)) {
                    fs::path dest = targetZone / entry.path().filename();
                    std::cout << "Copying: " << entry.path().string() << " -> " << dest.string() << std::endl;
                    fs::copy(entry.path(), dest, fs::copy_options::overwrite_existing);
                }
            }
            else {
                std::cout << "Source zone directory does not exist: " << sourceZone.string() << std::endl;
            }
        }
        catch(const std::exception& e) {
            std::cout << "Failed to copy zone files: " << e.what() << std::endl;
        }
        */

        // delete the old support file, to avoid some getting ui errors cause of two zone files
        fs::path supportFF = gamePath / "zone" / "support.ff";
        try {
            if(fs::exists(supportFF)) {
                std::cout << "Deleting: " << supportFF.string() << std::endl;
                fs::remove(supportFF);
            }
            else
                std::cout << "Failed to delete support.ff, it does not exists.." << std::endl;
        }
        catch(const std::exception& e) {
            std::cout << "Failed to delete support.ff: " << e.what() << std::endl;
        }

        fs::path dllPath = gamePath / "XInput9_1_0.dll";

        zip::Entry zipDllEntry;
        uint32_t existingDllCrc = 0;
        if (fs::exists(dllPath) &&
            zip::findEntry(zipPath.string(), "XInput9_1_0.dll", zipDllEntry) &&
            computeFileCrc32(dllPath, existingDllCrc) &&
            zipDllEntry.crc32 == existingDllCrc) {
            std::cout << "XInput9_1_0.dll already up to date, skipping extraction" << std::endl;
            return Result::Success;
        }

        try {
            std::cout << "Extracting XInput9_1_0.dll from " << zipPath.string() << std::endl;

            if (!zip::extractEntry(zipPath.string(), "XInput9_1_0.dll", dllPath.string())) {
                std::cout << "Extraction failed" << std::endl;
                return Result::ZipError;
            }

            if (!fs::exists(dllPath)) {
                std::cout << "DLL was not extracted successfully" << std::endl;
                return Result::FileNotFound;
            }

            std::cout << "Extraction successful" << std::endl;
            return Result::Success;
        }
        catch (const std::exception& e) {
            std::cout << "Exception: " << e.what() << std::endl;
            return Result::ZipError;
        }
        catch (...) {
            std::cout << "Unknown exception" << std::endl;
            return Result::ZipError;
        }
    }

    Result extractExe(const std::string& gameDir) {
        if (gameDir.empty()) {
            return Result::InvalidGamePath;
        }

        fs::path gamePath = fs::path(gameDir);
        fs::path gameExePath = gamePath / "BlackOps4.exe";

        std::cout << "Initial game path: " << gamePath.string() << std::endl;
        std::cout << "Looking for game exe at: " << gameExePath.string() << std::endl;

        if (!fs::exists(gameExePath)) {
            fs::path parentPath = gamePath.parent_path().parent_path();
            gameExePath = parentPath / "BlackOps4.exe";

            std::cout << "Trying parent path: " << parentPath.string() << std::endl;
            std::cout << "Looking for game exe at: " << gameExePath.string() << std::endl;

            if (!fs::exists(gameExePath)) {
                std::cout << "Game exe not found at parent path either" << std::endl;
                return Result::InvalidGamePath;
            }

            gamePath = parentPath;
            std::cout << "Updated game path to: " << gamePath.string() << std::endl;
        }

        fs::path launcherDir = gamePath / "project-bo4" / "launcher";
        fs::path zipPath = launcherDir / "exe.zip";
        std::cout << "Looking for zip at: " << zipPath.string() << std::endl;
        std::cout << "File exists: " << (fs::exists(zipPath) ? "Yes" : "No") << std::endl;

        if (!fs::exists(zipPath)) {
            return Result::FileNotFound;
        }

        zip::Entry zipExeEntry;
        uint32_t existingExeCrc = 0;
        if (zip::findEntry(zipPath.string(), "BlackOps4.exe", zipExeEntry) &&
            computeFileCrc32(gameExePath, existingExeCrc) &&
            zipExeEntry.crc32 == existingExeCrc) {
            std::cout << "BlackOps4.exe already up to date, skipping extraction" << std::endl;
            return Result::Success;
        }

        try {
            std::cout << "Extracting BlackOps4.exe from " << zipPath.string() << std::endl;

            if (!zip::extractEntry(zipPath.string(), "BlackOps4.exe", gameExePath.string())) {
                std::cout << "Extraction failed" << std::endl;
                return Result::ZipError;
            }

            if (!fs::exists(gameExePath)) {
                std::cout << "Exe was not extracted successfully" << std::endl;
                return Result::FileNotFound;
            }

            std::cout << "Extraction successful" << std::endl;
            return Result::Success;
        }
        catch (const std::exception& e) {
            std::cout << "Exception: " << e.what() << std::endl;
            return Result::ZipError;
        }
        catch (...) {
            std::cout << "Unknown exception" << std::endl;
            return Result::ZipError;
        }
    }
}
