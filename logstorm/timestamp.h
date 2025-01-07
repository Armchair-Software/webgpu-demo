#pragma once

#include <chrono>
#include <iomanip>
#include <sstream>

namespace logstorm {

class timestamp {
  std::chrono::time_point<std::chrono::system_clock> time_start{std::chrono::system_clock::now()};
public:
  enum class types {
    NONE,
    TIME,
    DATE,
    DATE_TIME,
    UNIX,
    SINCE_START,
    DEFAULT = NONE
  } type{types::DEFAULT};

  std::string operator()();

  explicit timestamp(types this_type = types::NONE);
  ~timestamp();
};

}
