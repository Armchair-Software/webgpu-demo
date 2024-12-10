#pragma once

#include <memory>
#include <type_traits>
#include "log_line_helper.h"

#ifdef __clang__
  #define CONSTEXPR_IF_NO_CLANG
#else
  #define CONSTEXPR_IF_NO_CLANG constexpr
#endif // __clang__ - see https://stackoverflow.com/questions/46576847/clang-vs-gcc-crtp-constexpr-variable-cannot-have-non-literal-type

namespace logstorm {

namespace sink {
class base;
}

class manager {
  /// Universal logger providing timestamping and simultaneous output to
  /// multiple independent sinks in either line-by-line or stream format.
  ///
  /// Usage:
  ///   logstorm::manager logger;
  ///   logger.add_sink<logstorm::sink::console>();
  ///   logger.add_sink(std::make_shared<logstorm::sink::file>("myfile.log", logstorm::timestamp::types::SINCE_START));
  ///   // different ways to log:
  ///   logger.log("hello world");
  ///   logger("hello world");
  ///   logger("hello ", "world ", 1234);
  ///   logger << "Hello world! " << 1234;   // note: newline is added automagically
private:
  std::vector<std::shared_ptr<sink::base>> sinks;                               // the output sinks we're logging to

public:
  template<typename T, class... Args, typename = std::enable_if_t<std::is_base_of<sink::base, T>::value>>
  size_t add_sink(Args&&... args);
  size_t add_sink(std::shared_ptr<sink::base> newsink);

  std::shared_ptr<sink::base> get_sink(size_t sink_id);

  void remove_sink(size_t sink_id);

  void clear_sinks();

  void log(std::string const &log_entry);

  template <typename T> inline CONSTEXPR_IF_NO_CLANG void operator()(T entry);
  template <typename... Args> inline CONSTEXPR_IF_NO_CLANG void operator()(Args&&... entries);
  template<typename T> inline CONSTEXPR_IF_NO_CLANG log_line_helper operator<<(T const &rhs);

  template<typename T, class... Args, typename = std::enable_if_t<std::is_base_of<sink::base, T>::value>>
  static logstorm::manager build_with_sink(Args&&... args);
};

template<typename T, class... Args, typename>
size_t manager::add_sink(Args&&... args) {
  return add_sink(std::make_shared<T>(args...));
}

template <typename T>
inline CONSTEXPR_IF_NO_CLANG void manager::operator()(T entry) {
  /// Convenience function to log a single entry
  log_line_helper helper(sinks);
  helper << entry;
}
template <typename... Args>
inline CONSTEXPR_IF_NO_CLANG void manager::operator()(Args&&... entries) {
  /// Convenience function to log any number of arguments
  log_line_helper helper(sinks);
  // now this is a hack... this is the hack of hacks.
  using unpack = int[];
  unpack{0, (helper << entries, 0)...};
}

template<typename T>
inline CONSTEXPR_IF_NO_CLANG log_line_helper manager::operator<<(T const &rhs) {
  /// Produce a log line helper and return it for further streaming
  log_line_helper helper(sinks);
  helper << rhs;
  return helper;
}

template<typename T, class... Args, typename>
logstorm::manager manager::build_with_sink(Args&&... args) {
  /// Build a manager class and initialise it with an initial sink
  logstorm::manager result;
  result.add_sink<T>(args...);
  return result;
}

}
