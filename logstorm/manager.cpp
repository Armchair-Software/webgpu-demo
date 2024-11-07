#include "manager.h"
#include "sink/base.h"

namespace logstorm {

size_t manager::add_sink(std::shared_ptr<sink::base> newsink) {
  /// Add a logging sink, and return its id for later reference
  sinks.emplace_back(newsink);
  sinks.shrink_to_fit();                                                        // we assume that adding sinks is an infrequent operation and minimising over-allocated memory is more important than avoiding reallocations here
  return sinks.size() - 1;
}

std::shared_ptr<sink::base> manager::get_sink(size_t sink_id) {
  /// Fetch a logging sink by its id
  return sinks.at(sink_id);
}

void manager::remove_sink(size_t sink_id) {
  /// Remove a logging sink by its id
  if(sink_id >= sinks.size()) {
    return;
  }
  sinks.erase(sinks.begin() + static_cast<ptrdiff_t>(sink_id));
  sinks.shrink_to_fit();
}

void manager::clear_sinks() {
  /// Remove all logging sinks
  sinks.clear();
}

void manager::log(std::string const &log_entry) {
  /// Log this line
  for(auto const &thissink : sinks) {
    thissink->log(log_entry);
  }
}

}
