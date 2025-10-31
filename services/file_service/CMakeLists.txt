cmake_minimum_required(VERSION 3.15)
project(file_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
  set(_GRPC_GRPCPP_LIB gRPC::grpc++)
else()
  set(_GRPC_GRPCPP_LIB ${gRPC_LIBRARIES})
endif()

if(TARGET protobuf::protoc)
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
elseif(DEFINED Protobuf_PROTOC_EXECUTABLE)
  set(_PROTOBUF_PROTOC ${Protobuf_PROTOC_EXECUTABLE})
else()
  find_program(_PROTOBUF_PROTOC protoc)
  if(NOT _PROTOBUF_PROTOC)
    message(FATAL_ERROR "protoc executable not found. Install protobuf compiler.")
  endif()
endif()

if(TARGET gRPC::grpc_cpp_plugin)
  set(_GRPC_CPP_PLUGIN $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
else()
  find_program(_GRPC_CPP_PLUGIN grpc_cpp_plugin)
  if(NOT _GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin executable not found. Install gRPC C++ plugin.")
  endif()
endif()

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/file_service.proto)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

set(PROTO_SRCS ${GEN_DIR}/file_service.pb.cc)
set(PROTO_HDRS ${GEN_DIR}/file_service.pb.h)
set(GRPC_SRCS ${GEN_DIR}/file_service.grpc.pb.cc)
set(GRPC_HDRS ${GEN_DIR}/file_service.grpc.pb.h)

add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
  COMMAND ${_PROTOBUF_PROTOC}
  ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
       --cpp_out=${GEN_DIR}
       --grpc_out=${GEN_DIR}
    --plugin=protoc-gen-grpc=${_GRPC_CPP_PLUGIN}
       ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  VERBATIM
)

add_executable(file_service
  src/FileServiceImpl.cpp
  src/main.cpp
  ${PROTO_SRCS}
  ${GRPC_SRCS}
)

add_executable(file_service_test_client
  src/FileServiceTestClient.cpp
  ${PROTO_SRCS}
  ${GRPC_SRCS}
)

set(_COMMON_INCLUDES
  ${GEN_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(Protobuf_INCLUDE_DIRS)
  list(APPEND _COMMON_INCLUDES ${Protobuf_INCLUDE_DIRS})
endif()

if(gRPC_INCLUDE_DIRS)
  list(APPEND _COMMON_INCLUDES ${gRPC_INCLUDE_DIRS})
endif()

target_include_directories(file_service PRIVATE ${_COMMON_INCLUDES})
target_include_directories(file_service_test_client PRIVATE ${_COMMON_INCLUDES})

target_link_libraries(file_service
  PRIVATE
    ${_GRPC_GRPCPP_LIB}
    ${_PROTOBUF_LIB}
)

target_link_libraries(file_service_test_client
  PRIVATE
    ${_GRPC_GRPCPP_LIB}
    ${_PROTOBUF_LIB}
)
