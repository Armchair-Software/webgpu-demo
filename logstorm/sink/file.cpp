#include "file.h"
#include <iostream>

namespace logstorm::sink {

file::file(std::string const &target_filename, timestamp::types timestamp_type)
  : base(timestamp_type),
    stream(target_filename, std::ios_base::app) {
  /// Default constructor
  if(!stream.good()) {
    std::cout << "LogStorm: WARNING: Couldn't open logfile" << target_filename << std::endl;
  }
}

file::~file() {
  /// Default destructor
  #ifndef LOGSTORM_SINGLE_THREADED
    std::scoped_lock lock(output_mutex);
  #endif // LOGSTORM_SINGLE_THREADED
  stream.close();
}

void file::log(std::string const &log_entry) {
  /// Log this line
  if(stream.good()) {
    #ifndef LOGSTORM_SINGLE_THREADED
      std::scoped_lock lock(output_mutex);
    #endif // LOGSTORM_SINGLE_THREADED
    stream << time() << log_entry << std::endl;
  }
}
void file::log_fragment(std::string const &log_entry) {
  /// Log this fragment without ending the line
  #ifdef LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
    if(line_in_progress.empty()) {                                              // if this is the start of a line, add a timestamp and cache it
      line_in_progress = time();
    }
    line_in_progress += log_entry;
    if(log_entry.back() == std::endl) {                                         // if this is a newline, push it to the buffer
      if(stream.good()) {
        stream << line_in_progress;
      }
      line_in_progress.clear();
    }
  #else
    if(stream.good()) {
      #ifndef LOGSTORM_SINGLE_THREADED
        std::scoped_lock lock(output_mutex);
      #endif // LOGSTORM_SINGLE_THREADED
      stream << log_entry;
    }
  #endif // LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
}

}
