#include "common/server_utils.h"

namespace common {

std::unique_ptr<grpc::Server> BuildServer(const std::string& listen_addr,
                                          grpc::Service* service) {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(listen_addr, grpc::InsecureServerCredentials());
  builder.RegisterService(service);
  return builder.BuildAndStart();
}

}  // namespace common