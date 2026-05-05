#include "Log.h"

#ifdef OPENSTEAMTOOL_LOGGING_ENABLED

#include "Config.h"
#include <atomic>
#include <filesystem>
#include <string>

namespace {
    std::atomic_bool g_mainReady{false};

    std::filesystem::path ResolveDllDir(HMODULE selfModule) {
        wchar_t buf[MAX_PATH] = {};
        DWORD len = GetModuleFileNameW(selfModule, buf, MAX_PATH);
        if (len == 0 || len == MAX_PATH) return L".";
        return std::filesystem::path(buf).parent_path();
    }

    spdlog::level::level_enum ToSpdlog(Config::LogLevel lv) {
        switch (lv) {
        case Config::LogLevel::Trace: return spdlog::level::trace;
        case Config::LogLevel::Debug: return spdlog::level::debug;
        case Config::LogLevel::Info:  return spdlog::level::info;
        case Config::LogLevel::Warn:  return spdlog::level::warn;
        case Config::LogLevel::Error: return spdlog::level::err;
        default: return spdlog::level::info;
        }
    }

    std::shared_ptr<spdlog::logger> MakeLogger(const std::string& dir,
                                                const std::string& name) {
        auto path = std::filesystem::path(dir) / (name + ".log");
        auto logger = spdlog::basic_logger_mt(name, path.string(), /*truncate=*/true);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [tid=%t] [%s:%# %!()] %v");
        logger->flush_on(spdlog::level::trace);
        return logger;
    }
}

namespace Log {

    void Init(HMODULE selfModule) {
        bool expected = false;
        if (!g_mainReady.compare_exchange_strong(expected, true)) return;

        try {
            auto dir = ResolveDllDir(selfModule);
            auto logDir = (dir / "opensteamtool").string();
            std::filesystem::create_directories(logDir);
            Main = MakeLogger(logDir, "main");
            Main->set_level(spdlog::level::trace);  // early boot: log everything
            LOG_INFO("Log initialised at {}", logDir);
        } catch (const std::exception&) {
            g_mainReady.store(false);
        }
    }

    void InitModules() {
        if (!g_mainReady) return;

        try {
            std::filesystem::create_directories(Config::logDir);
            auto lvl = ToSpdlog(Config::logLevel);

            Main->set_level(lvl);

            auto initOne = [&](std::shared_ptr<spdlog::logger>& logger, const char* name) {
                logger = MakeLogger(Config::logDir, name);
                logger->set_level(lvl);
            };

            #define OST_MOD(v, n) initOne(v, n);
            #include "ost_log_modules.h"
            #undef OST_MOD

            LOG_INFO("Module loggers initialised at {} level={}", Config::logDir,
                     static_cast<int>(Config::logLevel));
        } catch (const std::exception& e) {
            LOG_WARN("InitModules failed: {}", e.what());
        }
    }

}

#endif  // OPENSTEAMTOOL_LOGGING_ENABLED
