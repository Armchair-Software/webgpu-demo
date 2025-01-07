#pragma once

#include "base.h"
#include "logstorm/timestamp.h"

namespace logstorm::sink {

class emscripten_err : public base {
public:
  explicit emscripten_err(timestamp::types timestamp_type = timestamp::types::NONE);
  virtual ~emscripten_err() override;

  virtual void log(std::string const &log_entry) override final;
  virtual void log_fragment(std::string const &log_entry) override final;
};

}
