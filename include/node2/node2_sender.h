#ifndef NODE2_NODE2_SENDER_H_
#define NODE2_NODE2_SENDER_H_

#include <chrono>

#include "common/grpc_message_client.h"
#include "common/turn_coordinator.h"

namespace node2 {

class Node2Sender {
 public:
  Node2Sender(common::GrpcMessageClient& client,
              common::TurnCoordinator& coordinator,
              int rounds,
              std::chrono::milliseconds interval);

  void Run();

 private:
  common::GrpcMessageClient& client_;
  common::TurnCoordinator& coordinator_;
  int rounds_;
  std::chrono::milliseconds interval_;
};

}  // namespace node2

#endif  // NODE2_NODE2_SENDER_H_
