#ifndef COMMON_MESSENGER_SERVICE_H_
#define COMMON_MESSENGER_SERVICE_H_

#include <string>

#include <grpcpp/grpcpp.h>

#include "common/thread_safe_logger.h"
#include "pingpong.grpc.pb.h"

namespace common {

class MessengerService final : public pingpong::Messenger::Service {
 public:
  MessengerService(std::string self_name, ThreadSafeLogger& logger);

  grpc::Status SendMessage(grpc::ServerContext* context,
                           const pingpong::PingRequest* request,
                           pingpong::PingReply* reply) override;

 private:
  std::string self_name_;
  ThreadSafeLogger& logger_;
};

}  // namespace common

#endif  // COMMON_MESSENGER_SERVICE_H_