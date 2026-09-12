#pragma once

#include <chrono>
#include <cstdio>

namespace Base {

class ConsoleAdapter final {
  public:
    void log(const char* message) const noexcept { std::fputs(message, stderr); }
    void warning(const char* message) const noexcept { std::fputs(message, stderr); }

    template <typename... Arguments>
    void log(const char* format, Arguments... arguments) const noexcept {
        std::fprintf(stderr, format, arguments...);
    }

    template <typename... Arguments>
    void warning(const char* format, Arguments... arguments) const noexcept {
        std::fprintf(stderr, format, arguments...);
    }
};

inline ConsoleAdapter& Console() noexcept {
    static ConsoleAdapter console;
    return console;
}

class TimeElapsed final {
  public:
    TimeElapsed() noexcept : started_(std::chrono::steady_clock::now()) {}

    [[nodiscard]] static double diffTimeF(const TimeElapsed& first,
                                          const TimeElapsed& second) noexcept {
        return std::chrono::duration<double>(second.started_ - first.started_).count();
    }

  private:
    std::chrono::steady_clock::time_point started_;
};

} // namespace Base
