#include "node2/node2_sender.h"

#include <thread>

namespace node2 {

Node2Sender::Node2Sender(common::GrpcMessageClient& client,
                         common::TurnCoordinator& coordinator, int rounds,
                         std::chrono::seconds interval)
    : client_(client),
      coordinator_(coordinator),
      rounds_(rounds),
      interval_(interval) {}

void Node2Sender::Run() {
  for (int round = 1; round <= rounds_; ++round) {
    if (coordinator_.WaitFor(common::Turn::Node2ToNode1) ==
        common::Turn::Done) {
      return;
    }

    client_.Send("node2", "node1", round);
    std::this_thread::sleep_for(interval_);

    if (round == rounds_) {
      coordinator_.SetTurn(common::Turn::Done);
    } else {
      coordinator_.SetTurn(common::Turn::Node1ToNode2);
    }
  }
}

}  // namespace node2