cmake_minimum_required(VERSION 3.21)
project(LightMediaBrowser VERSION 0.0.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Concurrent Network)
find_package(SQLite3 REQUIRED)

add_executable(lightmediabrowser
    src/main.cpp
    src/domain/Video.h
    src/media/ThumbnailService.h
    src/media/ThumbnailService.cpp
    src/database/Database.h
    src/database/Database.cpp
    src/scanner/MediaScanner.h
    src/scanner/MediaScanner.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    resources/resources.qrc
)

target_include_directories(lightmediabrowser PRIVATE src)
target_link_libraries(lightmediabrowser PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Concurrent Qt6::Network SQLite::SQLite3)

install(TARGETS lightmediabrowser RUNTIME DESTINATION bin)
install(FILES resources/lightmediabrowser.desktop DESTINATION share/applications)
install(FILES resources/lightmediabrowser.png DESTINATION share/icons/hicolor/256x256/apps)
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION share/doc/lightmediabrowser)
