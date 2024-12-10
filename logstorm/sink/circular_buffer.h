#pragma once

#include "base.h"
#ifndef GUISTORM_SINGLETHREADED
  #include <shared_mutex>
#endif // GUISTORM_SINGLETHREADED
#include <boost/circular_buffer.hpp>

namespace logstorm::sink {

class circular_buffer : public base {
  /// A sink that stores strings in a circular buffer up to a fixed number of lines
public:
  unsigned int const size{256};
  boost::circular_buffer<std::string> data;
  #ifndef GUISTORM_SINGLETHREADED
    mutable std::shared_mutex data_mutex;
  #endif // GUISTORM_SINGLETHREADED

  circular_buffer(unsigned int max_lines, timestamp::types timestamp_type = timestamp::types::NONE);
  ~circular_buffer() override;

  virtual void log(std::string const &log_entry) override final;
  virtual void log_fragment(std::string const &log_entry) override final;
};

}
