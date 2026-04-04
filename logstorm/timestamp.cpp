#include "timestamp.h"
#include <ctime>
#ifndef LOGSTORM_SINGLE_THREADED
  #include <mutex>
#endif // LOGSTORM_SINGLE_THREADED

namespace logstorm {

namespace {

std::tm localtime_copy(std::time_t time) {
  #ifndef LOGSTORM_SINGLE_THREADED
    static std::mutex localtime_mutex;
    std::scoped_lock lock{localtime_mutex};
  #endif // LOGSTORM_SINGLE_THREADED
  return *std::localtime(&time);
}

} // anonymous namespace

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
      std::time_t time{std::time(nullptr)};
      std::tm time_info{localtime_copy(time)};
      std::stringstream ss;
      ss << std::put_time(&time_info, "%Y-%m-%d ");
      return ss.str();
    }
  case types::DATE:
    {
      std::time_t time{std::time(nullptr)};
      std::tm time_info{localtime_copy(time)};
      std::stringstream ss;
      ss << std::put_time(&time_info, "%H:%M:%S ");
      return ss.str();
    }
  case types::DATE_TIME:
    {
      std::time_t time{std::time(nullptr)};
      std::tm time_info{localtime_copy(time)};
      std::stringstream ss;
      ss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S ");
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
      std::chrono::duration<float> const time_elapsed{std::chrono::system_clock::now() - time_start};
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

} // namespace logstorm
