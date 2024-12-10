#pragma once

#include "base.h"
#include <fstream>
#ifndef LOGSTORM_SINGLE_THREADED
  #include <mutex>
#endif // LOGSTORM_SINGLE_THREADED
#include "logstorm/timestamp.h"

namespace logstorm::sink {

class file : public base {
  std::ofstream stream;
  #ifndef LOGSTORM_SINGLE_THREADED
    std::mutex output_mutex;
  #endif // LOGSTORM_SINGLE_THREADED

public:
  file(std::string const &target_filename, timestamp::types timestamp_type = timestamp::types::DATE_TIME);
  virtual ~file() override;

  virtual void log(std::string const &log_entry) override final;
  virtual void log_fragment(std::string const &log_entry) override final;
};

}
