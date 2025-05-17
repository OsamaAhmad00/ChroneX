#pragma once
#include <string>

namespace chronex::concepts {

template <typename LoggerType>
concept Logger = requires(LoggerType logger, std::string message) {
    // TODO Write different types of logger concepts
    logger.log(message);
};

}
