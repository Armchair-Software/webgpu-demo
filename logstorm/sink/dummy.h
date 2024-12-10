#pragma once

#include "base.h"
#include "logstorm/timestamp.h"

namespace logstorm::sink {

class dummy : public base {
  /// Dummy logging class to not do anything (for use in a non-logging environment)
public:
  explicit dummy(timestamp::types timestamp_type = timestamp::types::NONE);
  virtual ~dummy() override;

  virtual void log(std::string const &log_entry) override final;
  virtual void log_fragment(std::string const &log_entry) override final;
};

}
