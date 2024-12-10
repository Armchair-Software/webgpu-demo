#pragma once

#include <string>
#include "logstorm/timestamp.h"

namespace logstorm::sink {

class base {
  #ifdef LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY
    std::string line_in_progress;
  #endif // LOGSTORM_COMPOSE_FRAGMENTS_SEPARATELY

protected:
  timestamp time;

protected:
  explicit base(timestamp::types timestamp_type = timestamp::types::NONE);
public:
  virtual ~base();

  virtual void log(std::string const &log_entry) = 0;
  virtual void log_fragment(std::string const &log_entry) = 0;
};

}
