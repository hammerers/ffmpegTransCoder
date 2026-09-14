cmake_minimum_required(VERSION 3.20)
project(FFmpegVideoTransform VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# MSVC 编译选项优化与 UTF-8 字符集支持
if(MSVC)
    add_compile_options(/utf-8 /W4 /MP)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# 查找 Qt6 组件
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# 配置 FFmpeg 第三方依赖
set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ffmpeg")
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/sdl2")

add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
target_include_directories(FFmpeg::FFmpeg INTERFACE
    "${FFMPEG_DIR}/include"
)
target_link_libraries(FFmpeg::FFmpeg INTERFACE
    "${FFMPEG_DIR}/lib/avcodec.lib"
    "${FFMPEG_DIR}/lib/avformat.lib"
    "${FFMPEG_DIR}/lib/avutil.lib"
    "${FFMPEG_DIR}/lib/swscale.lib"
    "${FFMPEG_DIR}/lib/swresample.lib"
    "${FFMPEG_DIR}/lib/avfilter.lib"
    "${FFMPEG_DIR}/lib/avdevice.lib"
    "${FFMPEG_DIR}/lib/postproc.lib"
)

add_library(SDL2::SDL2 INTERFACE IMPORTED)
target_include_directories(SDL2::SDL2 INTERFACE
    "${SDL2_DIR}/include"
)
target_link_libraries(SDL2::SDL2 INTERFACE
    "${SDL2_DIR}/lib/SDL2.lib"
)

# 核心转码引擎与业务逻辑库
add_library(transcoder_core STATIC
    src/core/CommonTypes.h
    src/core/FFmpegUtils.h
    src/core/FFmpegUtils.cpp
    src/core/MetadataExtractor.h
    src/core/MetadataExtractor.cpp
    src/core/ThumbnailExtractor.h
    src/core/ThumbnailExtractor.cpp
    src/core/TranscodeEngine.h
    src/core/TranscodeEngine.cpp
    src/manager/TranscodeTask.h
    src/manager/TranscodeTask.cpp
    src/manager/TranscodeTaskManager.h
    src/manager/TranscodeTaskManager.cpp
)

target_include_directories(transcoder_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(transcoder_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
    FFmpeg::FFmpeg
    SDL2::SDL2
)

# 桌面客户端 GUI 主程序
add_executable(FFmpegVideoTransform WIN32
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/NavSidebar.h
    src/ui/NavSidebar.cpp
    src/ui/HomePage.h
    src/ui/HomePage.cpp
    src/ui/QueuePage.h
    src/ui/QueuePage.cpp
    src/ui/FilePrepPage.h
    src/ui/FilePrepPage.cpp
    src/ui/ParamConsolePage.h
    src/ui/ParamConsolePage.cpp
    src/ui/MediaInspectorPage.h
    src/ui/MediaInspectorPage.cpp
    src/ui/DropAreaWidget.h
    src/ui/DropAreaWidget.cpp
    src/ui/MediaInfoCard.h
    src/ui/MediaInfoCard.cpp
    src/ui/PresetPanel.h
    src/ui/PresetPanel.cpp
    src/ui/TaskItemWidget.h
    src/ui/TaskItemWidget.cpp
    src/ui/TaskListView.h
    src/ui/TaskListView.cpp
    src/resources/app.qrc
)

target_include_directories(FFmpegVideoTransform PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(FFmpegVideoTransform PRIVATE
    transcoder_core
    Qt6::Widgets
)

# 单元与集成测试程序 (控制台)
add_executable(test_transcoder
    tests/test_transcoder.cpp
)

target_link_libraries(test_transcoder PRIVATE
    transcoder_core
    Qt6::Core
    Qt6::Gui
)

# 拷贝 FFmpeg / SDL2 运行时 DLL 到输出目录
file(GLOB FFMPEG_DLLS "${FFMPEG_DIR}/bin/*.dll")

add_custom_command(TARGET FFmpegVideoTransform POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        $<TARGET_FILE_DIR:FFmpegVideoTransform>
    COMMENT "Copying FFmpeg and SDL2 runtime DLLs to application directory..."
)

add_custom_command(TARGET test_transcoder POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FFMPEG_DLLS}
        $<TARGET_FILE_DIR:test_transcoder>
    COMMENT "Copying FFmpeg and SDL2 runtime DLLs to test directory..."
)
