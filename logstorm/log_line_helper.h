#pragma once

#include <memory>
#include <vector>
#include <sstream>

namespace logstorm {

namespace sink {
class base;
}

class log_line_helper {
private:
  std::vector<std::shared_ptr<sink::base>> &sinks;
  std::stringstream aggregator;

public:
  explicit log_line_helper(std::vector<std::shared_ptr<sink::base>> &sinks_to_use);
  ~log_line_helper();

  log_line_helper(log_line_helper const &other);

  template<typename T> inline constexpr log_line_helper &operator<<(T const &rhs);
};

template<typename T>
inline constexpr log_line_helper &log_line_helper::operator<<(T const &rhs) {
  /// Input operator forwarding to all sinks in string format
  aggregator << rhs;
  return *this;
}

}
