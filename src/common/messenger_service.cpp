#include "common/messenger_service.h"

#include <sstream>

#include "common/time_utils.h"

namespace common {

MessengerService::MessengerService(std::string self_name, ThreadSafeLogger& logger)
    : self_name_(std::move(self_name)), logger_(logger) {}

grpc::Status MessengerService::SendMessage(grpc::ServerContext*,
                                           const pingpong::PingRequest* request,
                                           pingpong::PingReply* reply) {
  const std::string now = CurrentTimeString();

  std::ostringstream recv_log;
  recv_log << "[" << now << "] " << self_name_ << " received sequence="
           << request->sequence() << " from=" << request->from() << " body='"
           << request->body() << "'";
  logger_.Log(recv_log.str());

  reply->set_received(true);
  reply->set_ack_from(self_name_);
  reply->set_ack_at(now);
  reply->set_note("received");
  reply->set_sequence(request->sequence());

  return grpc::Status::OK;
}

}  // namespace common