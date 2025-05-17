#pragma once

namespace chronex::loggers {

class NullLogger {
public:
    template <typename T>
    void log(T&&) const noexcept { }
};

}