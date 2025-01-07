#include "emscripten_dbg_backtrace.h"
#ifdef __EMSCRIPTEN__
  // Note, all Emscripten-specific logging functions are noop in non-Emscripten builds
  #include <emscripten/console.h>
#endif //__EMSCRIPTEN__

namespace logstorm::sink {

emscripten_dbg_backtrace::emscripten_dbg_backtrace(timestamp::types timestamp_type)
  : base(timestamp_type) {
  /// Default constructor
}

emscripten_dbg_backtrace::~emscripten_dbg_backtrace() = default;

void emscripten_dbg_backtrace::log(std::string const &log_entry) {
  /// Log this line
  #ifdef __EMSCRIPTEN__
    ::emscripten_dbg_backtrace((time() + log_entry).c_str());
  #endif //__EMSCRIPTEN__
}
void emscripten_dbg_backtrace::log_fragment(std::string const &log_entry) {
  /// Log this fragment without ending the line
  #ifdef __EMSCRIPTEN__
    #ifdef LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
      if(line_in_progress.empty()) {                                            // if this is the start of a line, add a timestamp and cache it
        line_in_progress = time();
      }
      line_in_progress += log_entry;
      if(log_entry.back() == std::endl) {                                       // if this is a newline, push it to the buffer
        ::emscripten_dbg_backtrace(line_in_progress.c_str());
        line_in_progress.clear();
      }
    #else
      ::emscripten_dbg_backtrace(log_entry.c_str());
    #endif // LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
  #endif //__EMSCRIPTEN__
}

}
