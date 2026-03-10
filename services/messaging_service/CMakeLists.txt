cmake_minimum_required(VERSION 3.15)
project(messaging_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Dépendances (via vcpkg: -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(Threads REQUIRED)

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
  src/db/Database.cpp
  src/utils/Base64.cpp
  src/utils/EnvLoader.cpp
  ${PROTO_SRCS}
  ${GRPC_SRCS}
)

add_executable(test_client
  src/test_client.cpp
  ${PROTO_SRCS}
  ${GRPC_SRCS}
)

set(_COMMON_INCLUDES
  ${GEN_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/db
  ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

target_include_directories(messaging_service PRIVATE ${_COMMON_INCLUDES})
target_include_directories(test_client PRIVATE ${_COMMON_INCLUDES})

target_link_libraries(messaging_service
  PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    libpqxx::pqxx
    Threads::Threads
)

target_link_libraries(test_client
  PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)

# --- Tests unitaires ---
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()