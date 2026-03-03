cmake_minimum_required(VERSION 3.10)
project(zsy_learn_grpc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)

set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/proto/pingpong.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

if(DEFINED Protobuf_PROTOC_EXECUTABLE)
  set(PROTOC_EXECUTABLE "${Protobuf_PROTOC_EXECUTABLE}")
else()
  find_program(PROTOC_EXECUTABLE protoc REQUIRED)
endif()
find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)

find_package(gRPC CONFIG QUIET)
set(GRPC_LINK_LIBS)
set(GRPC_INCLUDE_DIRS)
if(gRPC_FOUND)
  list(APPEND GRPC_LINK_LIBS gRPC::grpc++)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GRPCPP REQUIRED grpc++)
  list(APPEND GRPC_LINK_LIBS ${GRPCPP_LIBRARIES})
  list(APPEND GRPC_INCLUDE_DIRS ${GRPCPP_INCLUDE_DIRS})
endif()

set(PROTO_SRCS "${GENERATED_DIR}/pingpong.pb.cc")
set(PROTO_HDRS "${GENERATED_DIR}/pingpong.pb.h")
set(GRPC_SRCS "${GENERATED_DIR}/pingpong.grpc.pb.cc")
set(GRPC_HDRS "${GENERATED_DIR}/pingpong.grpc.pb.h")

add_custom_command(
    OUTPUT "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
    COMMAND "${PROTOC_EXECUTABLE}"
    ARGS
        --proto_path "${CMAKE_CURRENT_SOURCE_DIR}/proto"
        --cpp_out "${GENERATED_DIR}"
        --grpc_out "${GENERATED_DIR}"
        --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
        "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
    COMMENT "Generating protobuf and gRPC sources"
    VERBATIM
)

add_library(pingpong_proto
    "${PROTO_SRCS}"
    "${GRPC_SRCS}"
)

target_include_directories(pingpong_proto
    PUBLIC
        "${GENERATED_DIR}"
        "${Protobuf_INCLUDE_DIRS}"
        "${GRPC_INCLUDE_DIRS}"
)

target_link_libraries(pingpong_proto
    PUBLIC
        ${GRPC_LINK_LIBS}
        ${Protobuf_LIBRARIES}
        Threads::Threads
)

add_executable(pingpong_demo src/main.cpp)
target_include_directories(pingpong_demo PRIVATE "${GENERATED_DIR}")
target_link_libraries(pingpong_demo PRIVATE pingpong_proto)