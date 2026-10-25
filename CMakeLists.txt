cmake_minimum_required(VERSION 3.21)
project(LightMediaBrowser VERSION 0.0.10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Concurrent Network)
find_package(SQLite3 REQUIRED)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(thumbnail_service_test
        tests/ThumbnailServiceTest.cpp
        src/media/ThumbnailService.h
        src/media/ThumbnailService.cpp
    )
    target_include_directories(thumbnail_service_test PRIVATE src)
    target_link_libraries(thumbnail_service_test PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
    add_test(NAME thumbnail_service_test COMMAND thumbnail_service_test)

    add_executable(database_test
        tests/DatabaseTest.cpp
        src/domain/Video.h
        src/database/Database.h
        src/database/Database.cpp
        resources/resources.qrc
    )
    target_include_directories(database_test PRIVATE src)
    target_link_libraries(database_test PRIVATE Qt6::Core Qt6::Sql Qt6::Test SQLite::SQLite3)
    add_test(NAME database_test COMMAND database_test)

    add_executable(media_scanner_test
        tests/MediaScannerTest.cpp
        src/domain/Video.h
        src/scanner/MediaScanner.h
        src/scanner/MediaScanner.cpp
    )
    target_include_directories(media_scanner_test PRIVATE src)
    target_link_libraries(media_scanner_test PRIVATE Qt6::Core Qt6::Concurrent Qt6::Test)
    add_test(NAME media_scanner_test COMMAND media_scanner_test)

    add_executable(video_metadata_probe_test
        tests/VideoMetadataProbeTest.cpp
        src/domain/Video.h
        src/media/VideoMetadataProbe.h
        src/media/VideoMetadataProbe.cpp
    )
    target_include_directories(video_metadata_probe_test PRIVATE src)
    target_link_libraries(video_metadata_probe_test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME video_metadata_probe_test COMMAND video_metadata_probe_test)
endif()

add_executable(lightmediabrowser
    src/main.cpp
    src/domain/Video.h
    src/media/ThumbnailService.h
    src/media/ThumbnailService.cpp
    src/media/VideoMetadataProbe.h
    src/media/VideoMetadataProbe.cpp
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
