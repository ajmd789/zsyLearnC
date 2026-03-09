#include "common/turn_coordinator.h"

namespace common {

TurnCoordinator::TurnCoordinator(Turn initial_turn) : turn_(initial_turn) {}

Turn TurnCoordinator::WaitFor(Turn expected_turn) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [&]() { return turn_ == expected_turn || turn_ == Turn::Done; });
  return turn_;
}

void TurnCoordinator::SetTurn(Turn next_turn) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (turn_ == Turn::Done && next_turn != Turn::Done) {
      return;
    }
    turn_ = next_turn;
  }
  cv_.notify_all();
}

}  // namespace common
