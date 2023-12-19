#pragma once

#include <format>
#include <rnu/thread_pool.hpp>
#include <thread>

namespace gev
{
  enum class severity
  {
    debug,
    verbose,
    warning,
    error
  };

  class logger
  {
  public:
    logger();

    template<typename... Args>
    void debug(std::format_string<Args...> const format, Args&&... args)
    {
      log(severity::debug, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void log(std::format_string<Args...> const format, Args&&... args)
    {
      log(severity::verbose, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(std::format_string<Args...> const format, Args&&... args)
    {
      log(severity::warning, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(std::format_string<Args...> const format, Args&&... args)
    {
      log(severity::error, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void log(severity s, std::format_string<Args...> const format, Args&&... args)
    {
      log_string(s, std::format(format, std::forward<Args>(args)...));
    }

  private:
    void log_string(severity s, std::string text);
    void write_log(severity s, std::string const& text);

    rnu::thread_pool _pool;
  };
}    // namespace gev