cmake_minimum_required(VERSION 3.10)
project(libmp4 C CXX)


include_directories(AFTER SYSTEM include libfutils/include/futils ulog/libulog/include)
file(GLOB SOURCE src/*.*)

message(${SOURCE})
add_library(mp4lib STATIC ${HEADERS} ${SOURCE})

add_executable(muxerjpeg tests/muxer_jpeg.cpp)
add_executable(muxerh264 tests/muxer_h264.cpp)
add_executable(demuxerjpeg tests/demuxer_jpeg.cpp)
add_executable(demuxerh264 tests/demuxer_h264.cpp)

# test programs
target_link_libraries(muxerjpeg mp4lib ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(muxerh264 mp4lib ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(demuxerjpeg mp4lib ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(demuxerh264 mp4lib ${CMAKE_THREAD_LIBS_INIT})

#install related
set_target_properties(mp4lib PROPERTIES PUBLIC_HEADER include/libmp4.h)
include(GNUInstallDirs)
install(TARGETS mp4lib
        PUBLIC_HEADER
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)