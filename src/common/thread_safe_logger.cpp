#include "common/thread_safe_logger.h"

#include <iostream>

namespace common {

void ThreadSafeLogger::Log(const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::cout << message << std::endl;
}

}  // namespace common