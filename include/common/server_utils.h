#ifndef COMMON_SERVER_UTILS_H_
#define COMMON_SERVER_UTILS_H_

#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

namespace common {

std::unique_ptr<grpc::Server> BuildServer(const std::string& listen_addr,
                                          grpc::Service* service);

}  // namespace common

#endif  // COMMON_SERVER_UTILS_H_