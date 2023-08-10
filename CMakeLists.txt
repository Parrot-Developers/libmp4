cmake_minimum_required(VERSION 3.15)
project(mp4 VERSION 1.0)


set(LIB_SOURCES
      src/mp4.c
      src/mp4_box_reader.c
      src/mp4_box_writer.c
      src/mp4_demux.c
      src/mp4_mux.c
      src/mp4_track.c
)

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCES})

# checks if set up rpath exists for install
if(COMMAND set_up_rpath)
    set_up_rpath()
else()
    message("Set up rpath not defined!")
endif()

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
set_target_properties(${PROJECT_NAME} PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

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
    EXPORT ${PROJECT_NAME}-targets
    PUBLIC_HEADER DESTINATION include
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

