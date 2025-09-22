cmake_minimum_required(VERSION 3.15)
project(auth_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Trouve les paquets via vcpkg
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# --- Fichier proto ---
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/auth.proto)

set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(PROTO_SRCS ${GENERATED_DIR}/auth.pb.cc)
set(PROTO_HDRS ${GENERATED_DIR}/auth.pb.h)
set(GRPC_SRCS ${GENERATED_DIR}/auth.grpc.pb.cc)
set(GRPC_HDRS ${GENERATED_DIR}/auth.grpc.pb.h)

# --- Commandes pour lancer protoc ---
add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
    COMMAND protobuf::protoc
    ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
         --cpp_out=${GENERATED_DIR}
         --grpc_out=${GENERATED_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    VERBATIM
)

# --- Cible serveur ---
add_executable(auth_service
    src/auth_service.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_include_directories(auth_service
    PRIVATE
        ${GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(auth_service
    gRPC::grpc++
    protobuf::libprotobuf
)
