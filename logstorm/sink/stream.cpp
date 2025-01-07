#include "stream.h"

namespace logstorm::sink {

stream::stream(std::ostream &target_ostream, timestamp::types timestamp_type)
  : base(timestamp_type),
    ostream(target_ostream) {
  /// Default constructor
}

stream::~stream() = default;

void stream::log(std::string const &log_entry) {
  /// Log this line
  #ifndef LOGSTORM_SINGLE_THREADED
    std::scoped_lock lock(output_mutex);
  #endif // LOGSTORM_SINGLE_THREADED
  ostream << time() << log_entry << std::endl;
}
void stream::log_fragment(std::string const &log_entry) {
  /// Log this fragment without ending the line
  #ifndef LOGSTORM_SINGLE_THREADED
    std::scoped_lock lock(output_mutex);
  #endif // LOGSTORM_SINGLE_THREADED
  #ifdef LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
    if(line_in_progress.empty()) {                                              // if this is the start of a line, add a timestamp and cache it
      line_in_progress = time();
    }
    line_in_progress += log_entry;
    if(log_entry.back() == '\n') {                                              // if this is a newline, push it to the buffer and flush
      if(stream.good()) {
        ostream << line_in_progress << std::flush;
      }
      line_in_progress.clear();
    } else if(log_entry.back() == std::endl) {                                  // if this is a newline with flush, push it to the buffer
      if(stream.good()) {
        ostream << line_in_progress;
      }
      line_in_progress.clear();
    }
  #else
    ostream << log_entry;
  #endif // LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
}

}
