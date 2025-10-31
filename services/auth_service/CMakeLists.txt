cmake_minimum_required(VERSION 3.15)
project(auth_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Fix spécifique à MSVC ---
if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

include(FetchContent)

# --- Dépendances (fallback module mode quand les fichiers CONFIG ne sont pas dispo) ---
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

FetchContent_Declare(jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.7.0
)

FetchContent_MakeAvailable(jwt-cpp)

if(TARGET jwt-cpp::jwt-cpp)
    set(_JWTCPP_TARGET jwt-cpp::jwt-cpp)
else()
    add_library(jwt-cpp-header-only INTERFACE)
    target_include_directories(jwt-cpp-header-only INTERFACE ${jwt-cpp_SOURCE_DIR}/include)
    set(_JWTCPP_TARGET jwt-cpp-header-only)
endif()

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
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
         --cpp_out=${GENERATED_DIR}
         --grpc_out=${GENERATED_DIR}
         --plugin=protoc-gen-grpc=${_GRPC_CPP_PLUGIN}
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    VERBATIM
)

# --- Sources C++ ---
set(SOURCES
    src/main.cpp
    src/AuthManager.cpp
    src/AuthServiceImpl.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

add_executable(auth_service
    ${SOURCES}
)

add_executable(auth_test_client
    src/auth_test_client.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

set(HEADERS
    src/AuthManager.h
    src/AuthServiceImpl.h
    ${PROTO_HDRS}
    ${GRPC_HDRS}
)

# --- Cible serveur ---
add_executable(auth_service ${SOURCES} ${HEADERS})

target_include_directories(auth_service
    PRIVATE
        ${GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(Protobuf_INCLUDE_DIRS)
    list(APPEND _COMMON_INCLUDES ${Protobuf_INCLUDE_DIRS})
endif()

if(gRPC_INCLUDE_DIRS)
    list(APPEND _COMMON_INCLUDES ${gRPC_INCLUDE_DIRS})
endif()

target_include_directories(auth_service PRIVATE ${_COMMON_INCLUDES})
target_include_directories(auth_test_client PRIVATE ${_COMMON_INCLUDES})

target_link_libraries(auth_service
    PRIVATE
        ${_GRPC_GRPCPP_LIB}
        ${_PROTOBUF_LIB}
        ${_JWTCPP_TARGET}
)

target_link_libraries(auth_test_client
    PRIVATE
        ${_GRPC_GRPCPP_LIB}
        ${_PROTOBUF_LIB}
        ${_JWTCPP_TARGET}
)
