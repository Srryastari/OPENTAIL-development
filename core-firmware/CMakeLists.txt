cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "0.1.32-alpha")
project(opentail_core_alpha)
# Scoped RMT allocation fix, after IDF has registered the pinned dependency.
include(cmake/led_strip_internal.cmake)
