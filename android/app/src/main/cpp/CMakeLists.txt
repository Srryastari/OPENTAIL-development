cmake_minimum_required(VERSION 3.22.1)
project(opentail_preview C)
set(CMAKE_C_STANDARD 11)
add_library(opentail_preview SHARED preview.c preview_jni.c
    core/wled_fx_compat.c core/effects_1d.c core/wled_1d.c
    core/palette_1d.c core/particle_system_1d.c core/ot_auto_effects.c)
target_include_directories(opentail_preview PRIVATE core)
target_compile_options(opentail_preview PRIVATE -O2 -Wall -Wextra -fvisibility=hidden)
target_link_libraries(opentail_preview m)
# Match platform page sizes without coupling the renderer to hardware drivers.
target_link_options(opentail_preview PRIVATE "-Wl,-z,max-page-size=16384")
