#include "node1/node1_sender.h"

#include <thread>

namespace node1 {

Node1Sender::Node1Sender(common::GrpcMessageClient& client,
                         common::TurnCoordinator& coordinator, int rounds,
                         std::chrono::milliseconds interval)
    : client_(client),
      coordinator_(coordinator),
      rounds_(rounds),
      interval_(interval) {}

void Node1Sender::Run() {
  for (int round = 1; round <= rounds_; ++round) {
    if (coordinator_.WaitFor(common::Turn::Node1ToNode2) ==
        common::Turn::Done) {
      return;
    }

    client_.Send("node1", "node2", round);
    std::this_thread::sleep_for(interval_);
    coordinator_.SetTurn(common::Turn::Node2ToNode1);
  }
}

}  // namespace node1
