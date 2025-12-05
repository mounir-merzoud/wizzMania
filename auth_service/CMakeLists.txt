cmake_minimum_required(VERSION 3.15)
project(auth_service LANGUAGES CXX)

# --- Configuration de base ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Intégration vcpkg (automatique via -DCMAKE_TOOLCHAIN_FILE) ---

# --- Dépendances ---
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(CryptoPP CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
add_definitions(-DJWT_CPP_VERSION_MAJOR=0)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(Threads REQUIRED)

# --- Génération Proto / GRPC ---
set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/auth.proto)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(PROTO_SRCS ${GENERATED_DIR}/auth.pb.cc)
set(PROTO_HDRS ${GENERATED_DIR}/auth.pb.h)
set(GRPC_SRCS ${GENERATED_DIR}/auth.grpc.pb.cc)
set(GRPC_HDRS ${GENERATED_DIR}/auth.grpc.pb.h)

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

# --- Recherche des sources ---
file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# --- Exécutable ---
add_executable(auth_service
    ${SRC_FILES}
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

# --- Dossiers d'inclusion ---
target_include_directories(auth_service
    PRIVATE
        ${GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/db
        ${CMAKE_CURRENT_SOURCE_DIR}/src/service
)

# --- Bibliothèques ---
target_link_libraries(auth_service
    PRIVATE
        gRPC::grpc++
        protobuf::libprotobuf
        libpqxx::pqxx
    cryptopp::cryptopp
        jwt-cpp::jwt-cpp
        Threads::Threads
)

# --- Message ---
message(STATUS "✅ Configuration terminée pour auth_service")
message(STATUS "  - Protobuf include dir: ${Protobuf_INCLUDE_DIRS}")
message(STATUS "  - gRPC include dir: ${gRPC_INCLUDE_DIRS}")
message(STATUS "  - Output dir: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
