#ifndef LOGSTORM_SINK_STREAM_H_INCLUDED
#define LOGSTORM_SINK_STREAM_H_INCLUDED

#include "base.h"
#include <ostream>
#ifndef LOGSTORM_SINGLE_THREADED
  #include <mutex>
#endif // LOGSTORM_SINGLE_THREADED
#include "logstorm/timestamp.h"

namespace logstorm::sink {

class stream : public base {
  std::ostream &ostream;
  #ifndef LOGSTORM_SINGLE_THREADED
    std::mutex output_mutex;
  #endif // LOGSTORM_SINGLE_THREADED

public:
  stream(std::ostream &target_ostream, timestamp::types timestamp_type = timestamp::types::NONE);
  virtual ~stream() override;

  virtual void log(std::string const &log_entry) override final;
  virtual void log_fragment(std::string const &log_entry) override final;
};

}

#endif // LOGSTORM_SINK_STREAM_H_INCLUDED
