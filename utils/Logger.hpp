#pragma once
#include <iostream>

namespace sc {

enum class LogLevel { Info, Warn, Error };

inline void log(LogLevel level, const std::string& msg) {
    switch (level) {
    case LogLevel::Info:
        std::cout << "[INFO] " << msg << std::endl;
        break;
    case LogLevel::Warn:
        std::cout << "[WARN] " << msg << std::endl;
        break;
    case LogLevel::Error:
        std::cerr << "[ERROR] " << msg << std::endl;
        break;
    }
}

} // namespace sc
