cmake_minimum_required(VERSION 3.15)
project(mp4 VERSION 1.0)

# checks if set up rpath exists for install

if(COMMAND set_up_rpath)
    set_up_rpath()
else()
    message("Set up rpath not defined!")
endif()

set(LIB_SOURCES
      src/mp4.c
      src/mp4_box_reader.c
      src/mp4_box_writer.c
      src/mp4_demux.c
      src/mp4_mux.c
      src/mp4_track.c
)

add_library(${PROJECT_NAME} ${LIB_SOURCES})

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

target_include_directories(${PROJECT_NAME} PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE "MP4_API_EXPORTS")
target_compile_options(${PROJECT_NAME} PRIVATE "-fvisibility=hidden")
target_compile_options(${PROJECT_NAME} PRIVATE "-std=gnu99")

if(WIN32)
  target_link_libraries(${PROJECT_NAME} ws2_32)
endif()

target_link_libraries(${PROJECT_NAME} futils ulog)

install(TARGETS ${PROJECT_NAME}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
)

