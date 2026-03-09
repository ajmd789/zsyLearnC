#include "app/pingpong_app.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "common/grpc_message_client.h"
#include "common/messenger_service.h"
#include "common/server_utils.h"
#include "common/thread_safe_logger.h"
#include "common/turn_coordinator.h"
#include "node1/node1_sender.h"
#include "node2/node2_sender.h"

namespace app {
namespace {

std::string MakeListenAddress(int port) {
  return "0.0.0.0:" + std::to_string(port);
}

std::string MakeTargetAddress(int port) {
  return "127.0.0.1:" + std::to_string(port);
}

std::string BuildStartSummary(const PingPongAppConfig& config) {
  std::ostringstream oss;
  oss << "starting ping-pong app"
      << " rounds=" << config.rounds
      << " interval_ms=" << config.interval.count()
      << " startup_wait_ms=" << config.startup_wait.count()
      << " node1_port=" << config.node1_port
      << " node2_port=" << config.node2_port;
  if (config.duration.has_value()) {
    oss << " duration_sec=" << config.duration->count();
  }
  return oss.str();
}

}  // namespace

PingPongApp::PingPongApp(PingPongAppConfig config) : config_(config) {}

int PingPongApp::Run() {
  common::ThreadSafeLogger logger;
  logger.Log(BuildStartSummary(config_));

  common::MessengerService node1_service("node1", logger);
  common::MessengerService node2_service("node2", logger);

  const std::string node1_server_addr = MakeListenAddress(config_.node1_port);
  const std::string node2_server_addr = MakeListenAddress(config_.node2_port);
  const std::string node1_target = MakeTargetAddress(config_.node1_port);
  const std::string node2_target = MakeTargetAddress(config_.node2_port);

  auto node1_server = common::BuildServer(node1_server_addr, &node1_service);
  auto node2_server = common::BuildServer(node2_server_addr, &node2_service);

  if (!node1_server || !node2_server) {
    logger.Log("failed to start gRPC servers");
    return 1;
  }

  std::thread node1_server_thread([&]() { node1_server->Wait(); });
  std::thread node2_server_thread([&]() { node2_server->Wait(); });

  std::this_thread::sleep_for(config_.startup_wait);

  common::GrpcMessageClient node1_client(node2_target, logger);
  common::GrpcMessageClient node2_client(node1_target, logger);

  common::TurnCoordinator coordinator(common::Turn::Node1ToNode2);
  node1::Node1Sender sender1(node1_client, coordinator, config_.rounds,
                             config_.interval);
  node2::Node2Sender sender2(node2_client, coordinator, config_.rounds,
                             config_.interval);

  std::mutex duration_mutex;
  std::condition_variable duration_cv;
  bool stop_duration_thread = false;

  std::thread duration_thread;
  if (config_.duration.has_value()) {
    duration_thread = std::thread([&]() {
      std::unique_lock<std::mutex> lock(duration_mutex);
      const bool stopped_early = duration_cv.wait_for(
          lock, *config_.duration, [&]() { return stop_duration_thread; });
      if (stopped_early) {
        return;
      }

      lock.unlock();
      logger.Log("duration limit reached, stopping ping-pong app");
      coordinator.SetTurn(common::Turn::Done);
      node1_server->Shutdown();
      node2_server->Shutdown();
    });
  }

  std::thread sender1_thread([&]() { sender1.Run(); });
  std::thread sender2_thread([&]() { sender2.Run(); });

  sender1_thread.join();
  sender2_thread.join();

  {
    std::lock_guard<std::mutex> lock(duration_mutex);
    stop_duration_thread = true;
  }
  duration_cv.notify_all();

  node1_server->Shutdown();
  node2_server->Shutdown();
  coordinator.SetTurn(common::Turn::Done);

  node1_server_thread.join();
  node2_server_thread.join();

  if (duration_thread.joinable()) {
    duration_thread.join();
  }

  logger.Log("ping-pong application stopped cleanly");
  return 0;
}

}  // namespace app
