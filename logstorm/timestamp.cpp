#include "timestamp.h"

namespace logstorm {

timestamp::timestamp(types this_type)
  : type(this_type) {
  /// Default constructor
}

timestamp::~timestamp() = default;

std::string timestamp::operator()() {
  /// Generate a timestamp as appropriate to this timestamp's type
  switch(type) {
  case types::NONE:
    return {};
  case types::TIME:
    {
      std::time_t time = std::time(nullptr);
      std::stringstream ss;
      ss << std::put_time(std::localtime(&time), "%Y-%m-%d ");
      return ss.str();
    }
  case types::DATE:
    {
      std::time_t time = std::time(nullptr);
      std::stringstream ss;
      ss << std::put_time(std::localtime(&time), "%H:%M:%S ");
      return ss.str();
    }
  case types::DATE_TIME:
    {
      std::time_t time = std::time(nullptr);
      std::stringstream ss;
      ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S ");
      return ss.str();
    }
  case types::UNIX:
    {
      std::stringstream ss;
      ss << std::time(nullptr) << " ";
      return ss.str();
    }
  case types::SINCE_START:
    {
      // give time in seconds to two decimal places
      std::chrono::duration<float> const time_elapsed = std::chrono::system_clock::now() - time_start;
      std::stringstream ss;
      ss << std::fixed << std::setprecision(2) << time_elapsed.count() << " ";
      return ss.str();
    }
  }
  #ifdef DISABLE_EXCEPTION_THROWING
    return {};
  #else // DISABLE_EXCEPTION_THROWING
    throw std::runtime_error("LogStorm: Invalid timestamp type " + std::to_string(static_cast<unsigned int>(type)));
  #endif // DISABLE_EXCEPTION_THROWING
}


}
