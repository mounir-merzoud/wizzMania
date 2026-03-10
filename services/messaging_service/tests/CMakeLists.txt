cmake_minimum_required(VERSION 3.15)

# --- Google Test via FetchContent ---
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# --- Sources du messaging_service (sans main.cpp) ---
set(MSG_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/db/Database.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/Base64.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/EnvLoader.cpp
)

# Fichiers proto générés
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/generated")

# --- Exécutable de test ---
add_executable(messaging_service_tests
    test_messaging_service.cpp
    ${MSG_SRC}
    ${GENERATED_DIR}/messaging.pb.cc
    ${GENERATED_DIR}/messaging.grpc.pb.cc
)

target_compile_definitions(messaging_service_tests
    PRIVATE MESSAGING_SERVICE_IMPL_ONLY
)

target_include_directories(messaging_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/db
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils
        ${GENERATED_DIR}
)

target_link_libraries(messaging_service_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        gRPC::grpc++
        protobuf::libprotobuf
        libpqxx::pqxx
        Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(messaging_service_tests)
