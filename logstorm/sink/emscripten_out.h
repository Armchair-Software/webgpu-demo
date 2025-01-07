#pragma once

#include "base.h"
#include "logstorm/timestamp.h"

namespace logstorm::sink {

class emscripten_out : public base {
public:
  explicit emscripten_out(timestamp::types timestamp_type = timestamp::types::NONE);
  virtual ~emscripten_out() override;

  virtual void log(std::string const &log_entry) override final;
  virtual void log_fragment(std::string const &log_entry) override final;
};

}
