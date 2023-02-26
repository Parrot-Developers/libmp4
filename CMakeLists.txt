cmake_minimum_required(VERSION 3.15)
project(libmp4 VERSION 1.0)

set(LIB_SOURCES
      src/mp4.c
      src/mp4_box_reader.c
      src/mp4_box_writer.c
      src/mp4_demux.c
      src/mp4_mux.c
      src/mp4_track.c
)

add_library(mp4 ${LIB_SOURCES})

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
set_target_properties(mp4 PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

target_include_directories(mp4 PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)

target_compile_definitions(mp4 PRIVATE "MP4_API_EXPORTS")
target_compile_options(mp4 PRIVATE "-fvisibility=hidden")
target_compile_options(mp4 PRIVATE "-std=gnu99")

if(WIN32)
  target_link_libraries(mp4 ws2_32)
endif()

target_link_libraries(mp4 futils ulog)