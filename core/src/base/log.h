// EmberMesh core — tiny leveled log hook. There is no global logger (no global mutable state):
// each component that logs holds a `Log` (a sink function + context + minimum level), which the
// owner (normally the node) configures. A default-constructed Log discards everything.
//
//   Log log;  log.set_sink(my_fn, my_ctx, LogLevel::Info);
//   log.info("mesh", "peer %s up on link %u", id.c_str(), link_id);
#pragma once

#include <cstdarg>
#include <cstdint>

namespace ember {

enum class LogLevel : uint8_t { Debug = 0, Info = 1, Warn = 2, Error = 3, Off = 4 };

const char* log_level_name(LogLevel l);

// `msg` is a NUL-terminated, already formatted line (no trailing newline), at most ~400 bytes.
using LogSink = void (*)(void* ctx, LogLevel level, const char* tag, const char* msg);

class Log {
 public:
  Log() = default;
  Log(LogSink sink, void* ctx, LogLevel min = LogLevel::Info) : sink_(sink), ctx_(ctx), min_(min) {}

  void set_sink(LogSink sink, void* ctx, LogLevel min = LogLevel::Info) {
    sink_ = sink;
    ctx_ = ctx;
    min_ = min;
  }
  void set_level(LogLevel min) { min_ = min; }
  bool enabled(LogLevel l) const { return sink_ && l >= min_ && l != LogLevel::Off; }

  void write(LogLevel l, const char* tag, const char* fmt, ...) const
#if defined(__GNUC__) || defined(__clang__)
      __attribute__((format(printf, 4, 5)))
#endif
      ;
  void vwrite(LogLevel l, const char* tag, const char* fmt, va_list ap) const;

#if defined(__GNUC__) || defined(__clang__)
#define EMBER_LOG_FMT __attribute__((format(printf, 3, 4)))
#else
#define EMBER_LOG_FMT
#endif
  void debug(const char* tag, const char* fmt, ...) const EMBER_LOG_FMT;
  void info(const char* tag, const char* fmt, ...) const EMBER_LOG_FMT;
  void warn(const char* tag, const char* fmt, ...) const EMBER_LOG_FMT;
  void error(const char* tag, const char* fmt, ...) const EMBER_LOG_FMT;
#undef EMBER_LOG_FMT

 private:
  LogSink sink_ = nullptr;
  void* ctx_ = nullptr;
  LogLevel min_ = LogLevel::Info;
};

}  // namespace ember
