cmake_minimum_required(VERSION 3.16)
project(desk_monitor_preview C CXX)
set(CMAKE_CXX_STANDARD 17)
set(LGFX "${CMAKE_CURRENT_SOURCE_DIR}/../.pio/libdeps/s3-eya-rgb-bounce/LovyanGFX/src")
if(NOT EXISTS "${LGFX}")
  set(LGFX "${CMAKE_CURRENT_SOURCE_DIR}/../.pio/libdeps/s3-eya-rgb/LovyanGFX/src")
endif()
find_package(SDL2 REQUIRED)
file(GLOB SOURCES
  "${LGFX}/lgfx/Fonts/efont/*.c" "${LGFX}/lgfx/Fonts/IPA/*.c"
  "${LGFX}/lgfx/Fonts/lvgl/*.c" "${LGFX}/lgfx/utility/*.c"
  "${LGFX}/lgfx/v1/*.cpp" "${LGFX}/lgfx/v1/lv_font/*.c"
  "${LGFX}/lgfx/v1/misc/*.cpp" "${LGFX}/lgfx/v1/platforms/sdl/*.cpp")
add_executable(preview main.cpp ${SOURCES}
  "${LGFX}/lgfx/v1/panel/Panel_Device.cpp"
  "${LGFX}/lgfx/v1/panel/Panel_FrameBufferBase.cpp")
target_compile_definitions(preview PRIVATE LGFX_SDL LGFX_USE_V1)
target_include_directories(preview PRIVATE "${LGFX}" ${SDL2_INCLUDE_DIRS})
target_link_libraries(preview PRIVATE ${SDL2_LIBRARIES} pthread)
