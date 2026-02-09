cmake_minimum_required(VERSION 3.15)
project(gateway_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/FS)
endif()

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# --- Fichiers proto ---
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Gateway proto
set(GATEWAY_PROTO ${PROTO_DIR}/gateway.proto)
set(GATEWAY_SRCS ${GENERATED_DIR}/gateway.pb.cc)
set(GATEWAY_HDRS ${GENERATED_DIR}/gateway.pb.h)
set(GATEWAY_GRPC_SRCS ${GENERATED_DIR}/gateway.grpc.pb.cc)
set(GATEWAY_GRPC_HDRS ${GENERATED_DIR}/gateway.grpc.pb.h)

add_custom_command(
    OUTPUT ${GATEWAY_SRCS} ${GATEWAY_HDRS} ${GATEWAY_GRPC_SRCS} ${GATEWAY_GRPC_HDRS}
    COMMAND protobuf::protoc
    ARGS --proto_path=${PROTO_DIR}
         --cpp_out=${GENERATED_DIR}
         --grpc_out=${GENERATED_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         ${GATEWAY_PROTO}
    DEPENDS ${GATEWAY_PROTO}
    VERBATIM
)

# Auth proto (use the real auth_service contract)
set(AUTH_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../auth_service/proto)
set(AUTH_PROTO ${AUTH_PROTO_DIR}/auth.proto)
set(AUTH_SRCS ${GENERATED_DIR}/auth.pb.cc)
set(AUTH_HDRS ${GENERATED_DIR}/auth.pb.h)
set(AUTH_GRPC_SRCS ${GENERATED_DIR}/auth.grpc.pb.cc)
set(AUTH_GRPC_HDRS ${GENERATED_DIR}/auth.grpc.pb.h)

add_custom_command(
    OUTPUT ${AUTH_SRCS} ${AUTH_HDRS} ${AUTH_GRPC_SRCS} ${AUTH_GRPC_HDRS}
    COMMAND protobuf::protoc
    ARGS --proto_path=${AUTH_PROTO_DIR}
         --proto_path=${PROTO_DIR}
         --cpp_out=${GENERATED_DIR}
         --grpc_out=${GENERATED_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         ${AUTH_PROTO}
    DEPENDS ${AUTH_PROTO}
    VERBATIM
)

# Messaging proto (from services/messaging_service)
set(MESSAGING_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../services/messaging_service/proto)
set(MESSAGING_PROTO ${MESSAGING_PROTO_DIR}/messaging.proto)
set(MESSAGING_SRCS ${GENERATED_DIR}/messaging.pb.cc)
set(MESSAGING_HDRS ${GENERATED_DIR}/messaging.pb.h)
set(MESSAGING_GRPC_SRCS ${GENERATED_DIR}/messaging.grpc.pb.cc)
set(MESSAGING_GRPC_HDRS ${GENERATED_DIR}/messaging.grpc.pb.h)

add_custom_command(
    OUTPUT ${MESSAGING_SRCS} ${MESSAGING_HDRS} ${MESSAGING_GRPC_SRCS} ${MESSAGING_GRPC_HDRS}
    COMMAND protobuf::protoc
    ARGS --proto_path=${PROTO_DIR}
         --proto_path=${MESSAGING_PROTO_DIR}
         --cpp_out=${GENERATED_DIR}
         --grpc_out=${GENERATED_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         ${MESSAGING_PROTO}
    DEPENDS ${MESSAGING_PROTO}
    VERBATIM
)

# --- Cible exécutable ---
add_executable(gateway_service
    src/main.cpp
    src/GatewayServiceImpl.cpp
    ${GATEWAY_SRCS} ${GATEWAY_GRPC_SRCS}
    ${AUTH_SRCS} ${AUTH_GRPC_SRCS}
    ${MESSAGING_SRCS} ${MESSAGING_GRPC_SRCS}
)

target_include_directories(gateway_service
    PRIVATE
        ${GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(gateway_service
    gRPC::grpc++
    protobuf::libprotobuf
)
