#ifndef NODE1_NODE1_SENDER_H_
#define NODE1_NODE1_SENDER_H_

#include <chrono>

#include "common/grpc_message_client.h"
#include "common/turn_coordinator.h"

namespace node1 {

class Node1Sender {
 public:
  Node1Sender(common::GrpcMessageClient& client,
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

}  // namespace node1

#endif  // NODE1_NODE1_SENDER_H_
