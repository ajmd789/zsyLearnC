#include <chrono>
#include <condition_variable>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "pingpong.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::InsecureChannelCredentials;
using grpc::InsecureServerCredentials;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

namespace {

std::mutex g_log_mutex;

std::string CurrentTimeString() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t tt = std::chrono::system_clock::to_time_t(now);

  std::tm tm_snapshot{};
#ifdef _WIN32
  localtime_s(&tm_snapshot, &tt);
#else
  localtime_r(&tt, &tm_snapshot);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm_snapshot, "%Y%m%d %H:%M:%S");
  return oss.str();
}

class MessengerService final : public pingpong::Messenger::Service {
 public:
  explicit MessengerService(std::string self_name) : self_name_(std::move(self_name)) {}

  Status SendMessage(ServerContext*, const pingpong::PingRequest* request,
                     pingpong::PingReply* reply) override {
    const std::string now = CurrentTimeString();

    {
      std::lock_guard<std::mutex> lock(g_log_mutex);
      std::cout << "[" << now << "] " << self_name_ << " received sequence=" << request->sequence()
                << " from=" << request->from() << " body='" << request->body() << "'"
                << std::endl;
    }

    reply->set_received(true);
    reply->set_ack_from(self_name_);
    reply->set_ack_at(now);
    reply->set_note("received");
    reply->set_sequence(request->sequence());

    return Status::OK;
  }

 private:
  std::string self_name_;
};

std::unique_ptr<Server> BuildServer(const std::string& listen_addr,
                                    pingpong::Messenger::Service* service) {
  ServerBuilder builder;
  builder.AddListeningPort(listen_addr, InsecureServerCredentials());
  builder.RegisterService(service);
  return builder.BuildAndStart();
}

bool SendOneMessage(pingpong::Messenger::Stub* stub, const std::string& from,
                    const std::string& to, int sequence) {
  pingpong::PingRequest request;
  request.set_from(from);
  request.set_to(to);
  request.set_body("hello from " + from);
  request.set_sent_at(CurrentTimeString());
  request.set_sequence(sequence);

  pingpong::PingReply reply;
  ClientContext context;
  const Status status = stub->SendMessage(&context, request, &reply);

  const std::string now = CurrentTimeString();
  std::lock_guard<std::mutex> lock(g_log_mutex);

  if (!status.ok()) {
    std::cout << "[" << now << "] " << from << " -> " << to << " sequence=" << sequence
              << " failed: " << status.error_message() << std::endl;
    return false;
  }

  std::cout << "[" << now << "] " << from << " -> " << to << " sequence=" << sequence
            << " ack=" << (reply.received() ? "true" : "false")
            << " ack_from=" << reply.ack_from() << " ack_at=" << reply.ack_at()
            << std::endl;
  return true;
}

}  // namespace

int main() {
  constexpr char kNode1Addr[] = "0.0.0.0:50051";
  constexpr char kNode2Addr[] = "0.0.0.0:50052";
  constexpr int kRounds = 10;
  constexpr auto kInterval = std::chrono::seconds(5);

  MessengerService node1_service("node1");
  MessengerService node2_service("node2");

  auto node1_server = BuildServer(kNode1Addr, &node1_service);
  auto node2_server = BuildServer(kNode2Addr, &node2_service);

  if (!node1_server || !node2_server) {
    std::cerr << "failed to start gRPC servers" << std::endl;
    return 1;
  }

  std::thread node1_server_thread([&node1_server]() { node1_server->Wait(); });
  std::thread node2_server_thread([&node2_server]() { node2_server->Wait(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  auto node1_to_node2 =
      pingpong::Messenger::NewStub(grpc::CreateChannel("127.0.0.1:50052", InsecureChannelCredentials()));
  auto node2_to_node1 =
      pingpong::Messenger::NewStub(grpc::CreateChannel("127.0.0.1:50051", InsecureChannelCredentials()));

  enum class Turn { Node1ToNode2, Node2ToNode1, Done };
  Turn turn = Turn::Node1ToNode2;

  std::mutex turn_mutex;
  std::condition_variable turn_cv;

  std::thread sender1([&]() {
    for (int round = 1; round <= kRounds; ++round) {
      std::unique_lock<std::mutex> lock(turn_mutex);
      turn_cv.wait(lock, [&]() { return turn == Turn::Node1ToNode2 || turn == Turn::Done; });
      if (turn == Turn::Done) {
        return;
      }
      lock.unlock();

      SendOneMessage(node1_to_node2.get(), "node1", "node2", round);
      std::this_thread::sleep_for(kInterval);

      lock.lock();
      turn = Turn::Node2ToNode1;
      lock.unlock();
      turn_cv.notify_all();
    }
  });

  std::thread sender2([&]() {
    for (int round = 1; round <= kRounds; ++round) {
      std::unique_lock<std::mutex> lock(turn_mutex);
      turn_cv.wait(lock, [&]() { return turn == Turn::Node2ToNode1 || turn == Turn::Done; });
      if (turn == Turn::Done) {
        return;
      }
      lock.unlock();

      SendOneMessage(node2_to_node1.get(), "node2", "node1", round);
      std::this_thread::sleep_for(kInterval);

      lock.lock();
      if (round == kRounds) {
        turn = Turn::Done;
      } else {
        turn = Turn::Node1ToNode2;
      }
      lock.unlock();
      turn_cv.notify_all();
    }
  });

  sender1.join();
  sender2.join();

  node1_server->Shutdown();
  node2_server->Shutdown();
  turn_cv.notify_all();

  node1_server_thread.join();
  node2_server_thread.join();

  std::cout << "completed " << kRounds << " ping-pong rounds" << std::endl;
  return 0;
}