#include "common/grpc_message_client.h"

#include <sstream>

#include <grpcpp/grpcpp.h>

#include "common/time_utils.h"

namespace common {

GrpcMessageClient::GrpcMessageClient(const std::string& target,
                                     ThreadSafeLogger& logger)
    : logger_(logger),
      stub_(pingpong::Messenger::NewStub(grpc::CreateChannel(
          target, grpc::InsecureChannelCredentials()))) {}

bool GrpcMessageClient::Send(const std::string& from, const std::string& to,
                             int sequence) {
  pingpong::PingRequest request;
  request.set_from(from);
  request.set_to(to);
  request.set_body("hello from " + from);
  request.set_sent_at(CurrentTimeString());
  request.set_sequence(sequence);

  pingpong::PingReply reply;
  grpc::ClientContext context;
  const grpc::Status status = stub_->SendMessage(&context, request, &reply);

  const std::string now = CurrentTimeString();
  std::ostringstream log_line;

  if (!status.ok()) {
    log_line << "[" << now << "] " << from << " -> " << to
             << " sequence=" << sequence
             << " failed: " << status.error_message();
    logger_.Log(log_line.str());
    return false;
  }

  log_line << "[" << now << "] " << from << " -> " << to
           << " sequence=" << sequence
           << " ack=" << (reply.received() ? "true" : "false")
           << " ack_from=" << reply.ack_from()
           << " ack_at=" << reply.ack_at();
  logger_.Log(log_line.str());
  return true;
}

}  // namespace common