cmake_minimum_required(VERSION 3.16)
project(SecureCloudDemo VERSION 0.1 LANGUAGES CXX)

# Utilise C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Indiquer où se trouve Qt (adapter le chemin si besoin)
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.2/mingw_64/lib/cmake")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Exécutable
add_executable(SecureCloudDemo
    main.cpp
)

# Lien avec Qt Widgets
target_link_libraries(SecureCloudDemo PRIVATE Qt6::Widgets)
