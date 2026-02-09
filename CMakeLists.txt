cmake_minimum_required(VERSION 3.16)
project(wizzMania VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configuration Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Trouver Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Network)

# Collecter tous les fichiers sources
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

# Ajouter le main.cpp à la racine
list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/main.cpp")

# Inclure les dossiers d'en-têtes
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/models
    ${CMAKE_SOURCE_DIR}/src/services
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/windows
    ${CMAKE_SOURCE_DIR}/src/ui/pages
    ${CMAKE_SOURCE_DIR}/src/ui/components
    ${CMAKE_SOURCE_DIR}/src/utils
)

# Créer l'exécutable
add_executable(wizzMania ${SOURCES})

# Lier les bibliothèques Qt
target_link_libraries(wizzMania
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
)

# Configuration macOS
if(APPLE)
    set_target_properties(wizzMania PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.laplateforme.wizzmania
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )
endif()
