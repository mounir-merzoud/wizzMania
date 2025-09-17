cmake_minimum_required(VERSION 3.16)
project(AuthService VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# httplib (HTTP server)
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)

# JSON (nlohmann)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

add_executable(auth_service main.cpp)

target_link_libraries(auth_service
  PRIVATE
    httplib
    nlohmann_json::nlohmann_json
)
