#include "common/time_utils.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace common {

std::string CurrentTimeString() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t tt = std::chrono::system_clock::to_time_t(now);

  std::tm tm_snapshot{};
#ifdef _WIN32
  localtime_s(&tm_snapshot, &tt);
#else
  localtime_r(&tt, &tm_snapshot);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm_snapshot, "%Y%m%d %H:%M:%S");
  return oss.str();
}

}  // namespace common