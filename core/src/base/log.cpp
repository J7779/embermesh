#include "base/log.h"

#include <cstdio>

namespace ember {

const char* log_level_name(LogLevel l) {
  switch (l) {
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warn: return "warn";
    case LogLevel::Error: return "error";
    case LogLevel::Off: return "off";
  }
  return "?";
}

void Log::vwrite(LogLevel l, const char* tag, const char* fmt, va_list ap) const {
  if (!enabled(l)) return;
  char buf[416];
  int n = std::vsnprintf(buf, sizeof buf, fmt, ap);
  if (n < 0) buf[0] = 0;
  sink_(ctx_, l, tag ? tag : "", buf);
}

void Log::write(LogLevel l, const char* tag, const char* fmt, ...) const {
  if (!enabled(l)) return;
  va_list ap;
  va_start(ap, fmt);
  vwrite(l, tag, fmt, ap);
  va_end(ap);
}

#define EMBER_LOG_IMPL(name, level)                              \
  void Log::name(const char* tag, const char* fmt, ...) const {  \
    if (!enabled(level)) return;                                 \
    va_list ap;                                                  \
    va_start(ap, fmt);                                           \
    vwrite(level, tag, fmt, ap);                                 \
    va_end(ap);                                                  \
  }
EMBER_LOG_IMPL(debug, LogLevel::Debug)
EMBER_LOG_IMPL(info, LogLevel::Info)
EMBER_LOG_IMPL(warn, LogLevel::Warn)
EMBER_LOG_IMPL(error, LogLevel::Error)
#undef EMBER_LOG_IMPL

}  // namespace ember
