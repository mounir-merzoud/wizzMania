cmake_minimum_required(VERSION 3.15)
project(messaging_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")

find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
  find_package(Protobuf REQUIRED)
endif()

find_package(gRPC CONFIG QUIET)
if(NOT gRPC_FOUND)
  find_package(gRPC REQUIRED)
endif()

if(TARGET protobuf::libprotobuf)
  set(_PROTOBUF_LIB protobuf::libprotobuf)
else()
  set(_PROTOBUF_LIB ${Protobuf_LIBRARIES})
endif()

if(TARGET gRPC::grpc++)
  set(_GRPC_GRPCPP gRPC::grpc++)
else()
  set(_GRPC_GRPCPP ${gRPC_LIBRARIES})
endif()

if(TARGET protobuf::protoc)
  set(_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
else()
  find_program(_PROTOC_EXECUTABLE protoc)
  if(NOT _PROTOC_EXECUTABLE)
    message(FATAL_ERROR "protoc executable not found. Install protobuf compiler.")
  endif()
endif()

if(TARGET gRPC::grpc_cpp_plugin)
  set(_GRPC_CPP_PLUGIN $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
else()
  find_program(_GRPC_CPP_PLUGIN grpc_cpp_plugin)
  if(NOT _GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin executable not found. Install gRPC plugins.")
  endif()
endif()

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/messaging.proto)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

set(PROTO_SRCS ${GEN_DIR}/messaging.pb.cc)
set(PROTO_HDRS ${GEN_DIR}/messaging.pb.h)
set(GRPC_SRCS ${GEN_DIR}/messaging.grpc.pb.cc)
set(GRPC_HDRS ${GEN_DIR}/messaging.grpc.pb.h)

add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
  COMMAND ${_PROTOC_EXECUTABLE}
  ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
       --cpp_out=${GEN_DIR}
       --grpc_out=${GEN_DIR}
       --plugin=protoc-gen-grpc=${_GRPC_CPP_PLUGIN}
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

set(_COMMON_INCLUDE_DIRS ${GEN_DIR})
if(Protobuf_INCLUDE_DIRS)
  list(APPEND _COMMON_INCLUDE_DIRS ${Protobuf_INCLUDE_DIRS})
endif()
if(gRPC_INCLUDE_DIRS)
  list(APPEND _COMMON_INCLUDE_DIRS ${gRPC_INCLUDE_DIRS})
endif()

target_include_directories(messaging_service PRIVATE ${_COMMON_INCLUDE_DIRS})
target_include_directories(test_client PRIVATE ${_COMMON_INCLUDE_DIRS})

target_link_libraries(messaging_service
  PRIVATE
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIB}
    pthread
)

target_link_libraries(test_client
  PRIVATE
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIB}
    pthread
)