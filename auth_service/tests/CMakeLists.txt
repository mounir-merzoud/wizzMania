cmake_minimum_required(VERSION 3.15)

# --- Google Test via FetchContent ---
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# Empêche googletest d'installer ses cibles dans le système
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# --- Fichiers sources de l'auth_service à compiler avec les tests ---
# On exclut main.cpp pour éviter la double définition de main()
file(GLOB_RECURSE AUTH_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cc"
)
list(FILTER AUTH_SRC EXCLUDE REGEX ".*/main\\.cpp$")

# Fichiers proto générés
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/generated")

# --- Exécutable de test ---
add_executable(auth_service_tests
    test_auth_service.cpp
    ${AUTH_SRC}
    ${GENERATED_DIR}/auth.pb.cc
    ${GENERATED_DIR}/auth.grpc.pb.cc
)

target_include_directories(auth_service_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/db
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/service
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils
        ${GENERATED_DIR}
)

target_link_libraries(auth_service_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        gRPC::grpc++
        protobuf::libprotobuf
        libpqxx::pqxx
        cryptopp::cryptopp
        jwt-cpp::jwt-cpp
        Threads::Threads
)

# --- Découverte automatique des tests par CTest ---
include(GoogleTest)
gtest_discover_tests(auth_service_tests)
