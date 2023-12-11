#include <gev/logger.hpp>
#include <iostream>

namespace gev
{
  std::string_view severity_name(severity s)
  {
    switch (s)
    {
    case severity::debug:
      return "[\x1B[32m debug \033[0m]";
    case severity::verbose:
      return "[\x1B[94mverbose\033[0m]";
    case severity::warning:
      return "[\x1B[33mwarning\033[0m]";
    case severity::error:
      return "[\x1B[31m error \033[0m]";
    default:
      return "[\x1B[90m ----- \033[0m]";
    }
  }

  void write_severity(std::ostream& out, severity s)
  {
    out << severity_name(s);
  }

  logger::logger()
  {
  }

  void logger::log_string(severity s, std::string text)
  {
    _pool.run_detached([this, s, t = std::move(text)] { write_log(s, t); });
  }

  void logger::write_log(severity s, std::string const& text)
  {
    write_severity(std::cout, s);
    std::cout << ' ' << text << "\n";
  }
}