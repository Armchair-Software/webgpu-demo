#include "console.h"
#include <iostream>

namespace logstorm::sink {

console::console(timestamp::types timestamp_type)
  : base(timestamp_type) {
  /// Default constructor
}

console::~console() = default;

void console::log(std::string const &log_entry) {
  /// Log this line
  #ifndef LOGSTORM_SINGLE_THREADED
    std::scoped_lock lock(output_mutex);
  #endif // LOGSTORM_SINGLE_THREADED
  std::cout << time() << log_entry << std::endl;
}
void console::log_fragment(std::string const &log_entry) {
  /// Log this fragment without ending the line
  #ifndef LOGSTORM_SINGLE_THREADED
    std::scoped_lock lock(output_mutex);
  #endif // LOGSTORM_SINGLE_THREADED
  #ifdef LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
    if(line_in_progress.empty()) {                                              // if this is the start of a line, add a timestamp and cache it
      line_in_progress = time();
    }
    line_in_progress += log_entry;
    if(log_entry.back() == std::endl) {                                         // if this is a newline, push it to the buffer
      std::cout << line_in_progress;
      line_in_progress.clear();
    }
  #else
    std::cout << log_entry;
  #endif // LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
}

}
