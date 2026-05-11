#include "dllmain.h"
#include "FileWatcher.h"
#include "LuaConfig.h"
#include "Hook/Hooks_Misc.h"
#include "Log.h"
#include <thread>
#include <atomic>
#include <vector>
#include <string>

namespace FileWatcher {
    static std::atomic<bool> g_running{false};
    static std::thread g_watcherThread;
    static std::vector<std::string> g_watchDirs;

    static const DWORD kDebounceMs = 500;

    static void TriggerRefresh(const std::vector<std::string>& newFiles) {
        LOG_PACKAGE_INFO("{} new Lua file(s) detected, refreshing after {}ms debounce",
                         newFiles.size(), kDebounceMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(kDebounceMs));

        for (const auto& path : newFiles)
            LuaConfig::ParseFile(path);

        Hooks_Misc::NotifyLicenseChanged();
        LOG_PACKAGE_INFO("Refresh completed");
    }

    // Collects newly created .lua filenames from the notification buffer.
    static std::vector<std::string> CollectNewLuaFiles(
        const char* buffer, DWORD bytesReturned, const std::string& dir)
    {
        std::vector<std::string> result;
        const FILE_NOTIFY_INFORMATION* info =
            reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer);
        while (info) {
            if (info->Action == FILE_ACTION_ADDED) {
                std::wstring_view fname(info->FileName, info->FileNameLength / sizeof(wchar_t));
                if (fname.size() >= 4 && fname.substr(fname.size() - 4) == L".lua") {
                    std::string name(fname.begin(), fname.end());
                    LOG_PACKAGE_INFO("New Lua file: {}", name);
                    result.push_back(dir + "\\" + name);
                }
            }
            if (info->NextEntryOffset == 0) break;
            info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<const char*>(info) + info->NextEntryOffset);
        }
        return result;
    }

    static bool IssueRead(HANDLE dir, char* buf, DWORD bufSize, OVERLAPPED* ov) {
        DWORD dummy = 0;
        if (!ReadDirectoryChangesW(dir, buf, bufSize, FALSE,
                                   FILE_NOTIFY_CHANGE_FILE_NAME,
                                   &dummy, ov, nullptr)) {
            if (GetLastError() != ERROR_IO_PENDING) {
                LOG_PACKAGE_WARN("ReadDirectoryChangesW failed: {}", GetLastError());
                return false;
            }
        }
        return true;
    }

    static void WatcherThread() {
        const size_t numDirs = g_watchDirs.size();
        std::vector<HANDLE> dirHandles(numDirs, nullptr);
        std::vector<OVERLAPPED> overlapped(numDirs);
        std::vector<HANDLE> events(numDirs, nullptr);
        std::vector<std::vector<char>> buffers(numDirs);

        for (size_t i = 0; i < numDirs; ++i) {
            events[i] = CreateEventA(nullptr, FALSE, FALSE, nullptr);
            overlapped[i].hEvent = events[i];
            buffers[i].resize(65536);

            dirHandles[i] = CreateFileA(
                g_watchDirs[i].c_str(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                nullptr);

            if (dirHandles[i] == INVALID_HANDLE_VALUE) {
                LOG_PACKAGE_WARN("Failed to open: {} (err={})", g_watchDirs[i], GetLastError());
                dirHandles[i] = nullptr;
                continue;
            }

            if (!IssueRead(dirHandles[i], buffers[i].data(),
                           static_cast<DWORD>(buffers[i].size()), &overlapped[i])) {
                CloseHandle(dirHandles[i]);
                dirHandles[i] = nullptr;
                continue;
            }

            LOG_PACKAGE_INFO("Watching: {}", g_watchDirs[i]);
        }

        bool allFailed = true;
        for (auto& h : dirHandles) if (h) { allFailed = false; break; }
        if (allFailed) {
            LOG_PACKAGE_WARN("No directories could be opened");
            for (auto& e : events) if (e) CloseHandle(e);
            return;
        }

        while (g_running) {
            DWORD waitResult = WaitForMultipleObjects(
                static_cast<DWORD>(numDirs), events.data(), FALSE, 1000);

            if (!g_running) break;
            if (waitResult == WAIT_TIMEOUT) continue;
            if (waitResult < WAIT_OBJECT_0 || waitResult >= WAIT_OBJECT_0 + numDirs) continue;

            size_t idx = waitResult - WAIT_OBJECT_0;
            HANDLE dir = dirHandles[idx];
            if (!dir) continue;

            DWORD bytesReturned = 0;
            if (GetOverlappedResult(dir, &overlapped[idx], &bytesReturned, FALSE)
                && bytesReturned > 0) {
                auto newFiles = CollectNewLuaFiles(
                    buffers[idx].data(), bytesReturned, g_watchDirs[idx]);
                if (!newFiles.empty())
                    TriggerRefresh(newFiles);
            }

            IssueRead(dir, buffers[idx].data(),
                      static_cast<DWORD>(buffers[idx].size()), &overlapped[idx]);
        }

        for (auto& h : dirHandles) if (h) CloseHandle(h);
        for (auto& e : events) if (e) CloseHandle(e);
        LOG_PACKAGE_INFO("Stopped");
    }

    void Start(const std::vector<std::string>& directories) {
        if (g_running.exchange(true)) {
            LOG_PACKAGE_WARN("Already running");
            return;
        }
        g_watchDirs = directories;
        g_watcherThread = std::thread(WatcherThread);
    }

    void Stop() {
        if (!g_running) return;
        g_running = false;
        if (g_watcherThread.joinable()) {
            g_watcherThread.join();
        }
    }
}
