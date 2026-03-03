#ifndef COMMON_GRPC_MESSAGE_CLIENT_H_
#define COMMON_GRPC_MESSAGE_CLIENT_H_

#include <memory>
#include <string>

#include "common/thread_safe_logger.h"
#include "pingpong.grpc.pb.h"

namespace common {

class GrpcMessageClient {
 public:
  GrpcMessageClient(const std::string& target, ThreadSafeLogger& logger);

  bool Send(const std::string& from, const std::string& to, int sequence);

 private:
  ThreadSafeLogger& logger_;
  std::unique_ptr<pingpong::Messenger::Stub> stub_;
};

}  // namespace common

#endif  // COMMON_GRPC_MESSAGE_CLIENT_H_