cmake_minimum_required(VERSION 3.16)
project(SensorChain VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.h
    mainwindow.cpp
    apiclient.h
    apiclient.cpp
    pages/dashboardpage.h
    pages/dashboardpage.cpp
    pages/devicespage.h
    pages/devicespage.cpp
    pages/sensorspage.h
    pages/sensorspage.cpp
    pages/adminpage.h
    pages/adminpage.cpp
    pages/historypage.h
    pages/historypage.cpp
    pages/blockchainexplorerpage.h
    pages/blockchainexplorerpage.cpp
    pages/consensusanalysispage.cpp
    pages/consensusanalysispage.h

)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(SensorChain
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(SensorChain SHARED ${PROJECT_SOURCES})
    else()
        add_executable(SensorChain ${PROJECT_SOURCES})
    endif()
endif()

target_link_libraries(SensorChain PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

set_target_properties(SensorChain PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS SensorChain
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(SensorChain)
endif()
