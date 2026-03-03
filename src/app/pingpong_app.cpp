#include "app/pingpong_app.h"

#include <chrono>
#include <memory>
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

constexpr char kNode1ServerAddr[] = "0.0.0.0:50051";
constexpr char kNode2ServerAddr[] = "0.0.0.0:50052";
constexpr char kNode1Target[] = "127.0.0.1:50051";
constexpr char kNode2Target[] = "127.0.0.1:50052";
constexpr int kRounds = 10;
const std::chrono::seconds kInterval(5);

}  // namespace

int PingPongApp::Run() {
  common::ThreadSafeLogger logger;

  common::MessengerService node1_service("node1", logger);
  common::MessengerService node2_service("node2", logger);

  auto node1_server = common::BuildServer(kNode1ServerAddr, &node1_service);
  auto node2_server = common::BuildServer(kNode2ServerAddr, &node2_service);

  if (!node1_server || !node2_server) {
    logger.Log("failed to start gRPC servers");
    return 1;
  }

  std::thread node1_server_thread([&]() { node1_server->Wait(); });
  std::thread node2_server_thread([&]() { node2_server->Wait(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  common::GrpcMessageClient node1_client(kNode2Target, logger);
  common::GrpcMessageClient node2_client(kNode1Target, logger);

  common::TurnCoordinator coordinator(common::Turn::Node1ToNode2);
  node1::Node1Sender sender1(node1_client, coordinator, kRounds, kInterval);
  node2::Node2Sender sender2(node2_client, coordinator, kRounds, kInterval);

  std::thread sender1_thread([&]() { sender1.Run(); });
  std::thread sender2_thread([&]() { sender2.Run(); });

  sender1_thread.join();
  sender2_thread.join();

  node1_server->Shutdown();
  node2_server->Shutdown();
  coordinator.SetTurn(common::Turn::Done);

  node1_server_thread.join();
  node2_server_thread.join();

  logger.Log("completed 10 ping-pong rounds");
  return 0;
}

}  // namespace app