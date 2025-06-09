#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cstdlib>

namespace sc {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

inline LogLevel &globalLogLevel() {
    static LogLevel level = [] {
        const char *env = std::getenv("SC_LOG_LEVEL");
        if (!env)
            return LogLevel::Info;
        std::string val(env);
        if (val == "DEBUG")
            return LogLevel::Debug;
        if (val == "WARN")
            return LogLevel::Warn;
        if (val == "ERROR")
            return LogLevel::Error;
        return LogLevel::Info;
    }();
    return level;
}

inline void setLogLevel(LogLevel level) { globalLogLevel() = level; }

inline const char *levelTag(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}

inline std::string currentTime() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%F %T");
    return ss.str();
}

inline void log(LogLevel level, const std::string &msg,
                const char *file = nullptr, int line = 0) {
    if (static_cast<int>(level) < static_cast<int>(globalLogLevel()))
        return;
    std::ostream &out = (level == LogLevel::Error ? std::cerr : std::cout);
    out << '[' << levelTag(level) << "] " << currentTime();
    if (file)
        out << ' ' << file << ':' << line;
    out << " " << msg << std::endl;
}

} // namespace sc

#define SC_LOG(level, msg) ::sc::log(level, msg, __FILE__, __LINE__)
