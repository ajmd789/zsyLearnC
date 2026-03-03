#ifndef COMMON_THREAD_SAFE_LOGGER_H_
#define COMMON_THREAD_SAFE_LOGGER_H_

#include <mutex>
#include <string>

namespace common {

class ThreadSafeLogger {
 public:
  void Log(const std::string& message);

 private:
  std::mutex mutex_;
};

}  // namespace common

#endif  // COMMON_THREAD_SAFE_LOGGER_H_