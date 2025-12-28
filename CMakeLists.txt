cmake_minimum_required(VERSION 3.10)
project(shoot_commands VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann/json.hpp")
    message(FATAL_ERROR "Brak pliku nagłówkowego nlohmann/json.hpp.")
endif()
include_directories(nlohmann)

add_executable(${PROJECT_NAME}
    main.cpp
    mainwindow.cpp
    mainwindow.h
    commandexecutor.cpp
    commandexecutor.h
    settingsdialog.cpp
    settingsdialog.h
    sequencerunner.cpp
    sequencerunner.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
)
