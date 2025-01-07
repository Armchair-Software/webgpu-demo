#include "base.h"

namespace logstorm::sink {

base::base(timestamp::types timestamp_type)
  : time(timestamp_type) {
  /// Default constructor
}

base::~base() = default;

}
