#pragma once

#include "base.h"
#include <fstream>
#ifndef LOGSTORM_SINGLE_THREADED
  #include <mutex>
#endif // LOGSTORM_SINGLE_THREADED
#include "logstorm/timestamp.h"

namespace logstorm::sink {

class fstream : public base {
  std::ofstream &stream;
  #ifndef LOGSTORM_SINGLE_THREADED
    std::mutex output_mutex;
  #endif // LOGSTORM_SINGLE_THREADED

public:
  fstream(std::ofstream &target_stream, timestamp::types timestamp_type = timestamp::types::DATE_TIME);
  virtual ~fstream() override;

  virtual void log(std::string const &log_entry) override final;
  virtual void log_fragment(std::string const &log_entry) override final;
};

}
