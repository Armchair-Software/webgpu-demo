#include "log_line_helper.h"
#include <iostream>
#include "sink/base.h"

namespace logstorm {

log_line_helper::log_line_helper(std::vector<std::shared_ptr<sink::base>> &sinks_to_use)
  : sinks(sinks_to_use) {
  /// Default constructor
}

log_line_helper::log_line_helper(log_line_helper const &other)
  : sinks(other.sinks) {
  /// Copy constructor
  std::cout << "LogStorm: WARNING: Return value optimisation appears to have failed, copy constructor called - log entries may be duplicated." << std::endl;
}

log_line_helper::~log_line_helper() {
  /// Default destructor
  // output all lines in one go when we destruct
  for(auto const &thissink : sinks) {
    thissink->log(aggregator.str());
  }
}

}
