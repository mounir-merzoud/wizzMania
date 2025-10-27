cmake_minimum_required(VERSION 3.15)
project(messaging_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/messaging.proto)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

set(PROTO_SRCS ${GEN_DIR}/messaging.pb.cc)
set(PROTO_HDRS ${GEN_DIR}/messaging.pb.h)
set(GRPC_SRCS ${GEN_DIR}/messaging.grpc.pb.cc)
set(GRPC_HDRS ${GEN_DIR}/messaging.grpc.pb.h)

add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
  COMMAND protobuf::protoc
  ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
       --cpp_out=${GEN_DIR}
       --grpc_out=${GEN_DIR}
       --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
       ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  VERBATIM
)

add_executable(messaging_service
  src/messaging_service.cpp
  ${PROTO_SRCS}
  ${GRPC_SRCS}
)

add_executable(test_client
  src/test_client.cpp
  ${PROTO_SRCS}
  ${GRPC_SRCS}
)

target_include_directories(messaging_service PRIVATE ${GEN_DIR})
target_include_directories(test_client PRIVATE ${GEN_DIR})

target_link_libraries(messaging_service
  gRPC::grpc++
  protobuf::libprotobuf
  pthread
)

target_link_libraries(test_client
  gRPC::grpc++
  protobuf::libprotobuf
  pthread
)