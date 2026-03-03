#ifndef COMMON_TURN_COORDINATOR_H_
#define COMMON_TURN_COORDINATOR_H_

#include <condition_variable>
#include <mutex>

namespace common {

enum class Turn {
  Node1ToNode2,
  Node2ToNode1,
  Done,
};

class TurnCoordinator {
 public:
  explicit TurnCoordinator(Turn initial_turn = Turn::Node1ToNode2);

  Turn WaitFor(Turn expected_turn);
  void SetTurn(Turn next_turn);

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  Turn turn_;
};

}  // namespace common

#endif  // COMMON_TURN_COORDINATOR_H_