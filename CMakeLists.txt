cmake_minimum_required(VERSION 3.16)
project(iso-dispatcher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Widgets)

add_executable(iso-dispatcher
    src/main.cpp

    src/core/Types.h
    src/core/Config.h        src/core/Config.cpp
    src/core/IsoProbe.h      src/core/IsoProbe.cpp
    src/core/Classifier.h    src/core/Classifier.cpp
    src/core/Knowledge.h     src/core/Knowledge.cpp
    src/core/Launcher.h      src/core/Launcher.cpp
    src/core/MarkerMaker.h   src/core/MarkerMaker.cpp
    src/core/Util.h          src/core/Util.cpp

    src/cli/Cli.h            src/cli/Cli.cpp
    src/ui/AskDialog.h       src/ui/AskDialog.cpp
    src/ui/ConfigDialog.h    src/ui/ConfigDialog.cpp
    src/ui/MainWindow.h      src/ui/MainWindow.cpp

    src/app/App.h            src/app/App.cpp
)

target_include_directories(iso-dispatcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(iso-dispatcher PRIVATE Qt6::Core Qt6::Widgets)

install(TARGETS iso-dispatcher RUNTIME DESTINATION bin)
