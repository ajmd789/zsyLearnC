#ifndef APP_PINGPONG_APP_H_
#define APP_PINGPONG_APP_H_

#include <chrono>
#include <optional>

namespace app {

struct PingPongAppConfig {
  int rounds = 10;
  std::chrono::milliseconds interval{5000};
  std::chrono::milliseconds startup_wait{500};
  std::optional<std::chrono::seconds> duration;
  int node1_port = 50051;
  int node2_port = 50052;
};

class PingPongApp {
 public:
  explicit PingPongApp(PingPongAppConfig config = {});

  int Run();

 private:
  PingPongAppConfig config_;
};

}  // namespace app

#endif  // APP_PINGPONG_APP_H_
