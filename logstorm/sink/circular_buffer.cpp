#include "circular_buffer.h"
#ifndef LOGSTORM_SINGLE_THREADED
  #include <mutex>
#endif // LOGSTORM_SINGLE_THREADED

namespace logstorm::sink {

circular_buffer::circular_buffer(unsigned int this_size, timestamp::types timestamp_type)
  : base(timestamp_type),
    size(this_size),
    data(size) {
  /// Default constructor
}

circular_buffer::~circular_buffer() = default;

void circular_buffer::log(std::string const &log_entry) {
  /// Log this line
  #ifndef LOGSTORM_SINGLE_THREADED
    std::unique_lock lock(data_mutex);                                          // lock for writing (unique)
  #endif // LOGSTORM_SINGLE_THREADED
  data.push_back(time() + log_entry);
}
void circular_buffer::log_fragment(std::string const &log_entry) {
  /// Log this fragment without ending the line
  #ifndef LOGSTORM_SINGLE_THREADED
    std::unique_lock lock(data_mutex);                                          // lock for writing (unique)
  #endif // LOGSTORM_SINGLE_THREADED
  #ifdef LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
    if(line_in_progress.empty()) {                                              // if this is the start of a line, add a timestamp and cache it
      line_in_progress = time();
    }
    line_in_progress += log_entry;
    if(log_entry.back() == std::endl) {                                         // if this is a newline, push it to the buffer
      data.push_back(line_in_progress);
      line_in_progress.clear();
    }
  #else
    data.back() += log_entry;
  #endif // LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
}

}
