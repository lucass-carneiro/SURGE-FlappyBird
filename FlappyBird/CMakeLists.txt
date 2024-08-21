cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# -----------------------------------------
# Project
# -----------------------------------------

project(
  SurgeFlappyBird
  VERSION 1.0.0
  LANGUAGES CXX
)

# -----------------------------------------
#  Target sources
# -----------------------------------------

set(
  SURGE_MODULE_FLAPPY_BIRD_HEADER_LIST
  "${SurgeCore_SOURCE_DIR}/include/surge_core.hpp"

  "${PROJECT_SOURCE_DIR}/include/flappy_bird.hpp"
)

set(
  SURGE_MODULE_FLAPPY_BIRD_SOURCE_LIST
  "${PROJECT_SOURCE_DIR}/src/flappy_bird.cpp"
  "${PROJECT_SOURCE_DIR}/src/state_machine.cpp"
  "${PROJECT_SOURCE_DIR}/src/state_update.cpp"
)

# -----------------------------------------
# Module Target
# -----------------------------------------

add_library(SurgeFlappyBird MODULE ${SURGE_MODULE_FLAPPY_BIRD_HEADER_LIST} ${SURGE_MODULE_FLAPPY_BIRD_SOURCE_LIST})
target_compile_features(SurgeFlappyBird PRIVATE cxx_std_20)
set_target_properties(SurgeFlappyBird PROPERTIES PREFIX "")
set_target_properties(SurgeFlappyBird PROPERTIES OUTPUT_NAME "FlappyBird")
set_target_properties(SurgeFlappyBird PROPERTIES PUBLIC_HEADER "${PROJECT_SOURCE_DIR}/include/flappy_bird.hpp")
set_target_properties(SurgeFlappyBird PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_compile_definitions(SurgeFlappyBird PUBLIC COMPILING_SURGE_MODULE_FLAPPY_BIRD)

target_include_directories(
  SurgeFlappyBird PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

# Enables __VA_OPT__ on msvc
if(SURGE_COMPILER_FLAG_STYLE MATCHES "msvc")
    target_compile_options(SurgeFlappyBird PUBLIC /Zc:preprocessor)
endif()

# Use UTF-8 on MSVC
if(SURGE_COMPILER_FLAG_STYLE MATCHES "msvc")
    target_compile_options(SurgeFlappyBird PUBLIC /utf-8)
endif()

# Disable min/max macros on msvc
if(SURGE_COMPILER_FLAG_STYLE MATCHES "msvc")
    target_compile_options(SurgeFlappyBird PUBLIC /D NOMINMAX)
endif()

# -----------------------------------------
# Compilers flags and options
# -----------------------------------------

if(SURGE_ENABLE_SANITIZERS)
  if(SURGE_COMPILER_FLAG_STYLE MATCHES "gcc")
    target_compile_options(SurgeFlappyBird PUBLIC -fsanitize=address,null,unreachable,undefined)
    target_link_options(SurgeFlappyBird PUBLIC -fsanitize=address,null,unreachable,undefined)
  else()
    message(WARNING "Sanitizers don't work on MSVC yet.")
  endif()
endif()

if(SURGE_ENABLE_OPTIMIZATIONS)
  if(SURGE_COMPILER_FLAG_STYLE MATCHES "gcc")
    target_compile_options(SurgeFlappyBird PUBLIC -O2)
    target_link_options(SurgeFlappyBird PUBLIC -O2)
  else()
    target_compile_options(SurgeFlappyBird PUBLIC /O2)
  endif()
endif()

if(SURGE_ENABLE_TUNING)
  if(SURGE_COMPILER_FLAG_STYLE MATCHES "gcc")
    target_compile_options(SurgeFlappyBird PUBLIC -march=native -mtune=native)
    target_link_options(SurgeFlappyBird PUBLIC -march=native -mtune=native)
  else()
    message(WARNING "TODO: Unknow tuning flags for msvc")
  endif()
endif()

if(SURGE_ENABLE_LTO)
  if(SURGE_COMPILER_FLAG_STYLE MATCHES "gcc")
    target_compile_options(SurgeFlappyBird PUBLIC -flto)
    target_link_options(SurgeFlappyBird PUBLIC -flto)
  else()
    message(WARNING "TODO: Unknow LTO flag for msvc")    
  endif()
endif()

if(SURGE_ENABLE_FAST_MATH)
  if(SURGE_COMPILER_FLAG_STYLE MATCHES "gcc")
    target_compile_options(SurgeFlappyBird PUBLIC -ffast-math)
    target_link_options(SurgeFlappyBird PUBLIC -ffast-math)
  else()
    target_compile_options(SurgeFlappyBird PUBLIC /fp:fast)
  endif()
endif()

# Dependencies and main program build type must match. If building dependencies static-debug, then /MTd must be used.
# See the link bellow for further information
# https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2012/2kzt1wy3(v=vs.110)?redirectedfrom=MSDN

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Generating a Debug build system")

  if(SURGE_COMPILER_FLAG_STYLE MATCHES "gcc")
    target_compile_options(
      SurgeFlappyBird
      PUBLIC
      -Og
      -g3
      -ggdb3
      -fno-omit-frame-pointer
      -Werror
      -Wall
      -Wextra
      -Wpedantic
      -Walloca
      -Wcast-qual
      -Wformat=2
      -Wformat-security
      -Wnull-dereference
      -Wstack-protector
      -Wvla
      -Wconversion
      -Warray-bounds
      -Warray-bounds-pointer-arithmetic
      -Wconditional-uninitialized
      -Wimplicit-fallthrough
      -Wpointer-arith
      -Wformat-type-confusion
      -Wfloat-equal
      -Wassign-enum
      -Wtautological-constant-in-range-compare
      -Wswitch-enum
      -Wshift-sign-overflow
      -Wloop-analysis
      -Wno-switch-enum
    )
      target_link_options(SurgeFlappyBird PUBLIC -Og -g3 -ggdb3)
  else()
    target_compile_options(SurgeFlappyBird PUBLIC /Wall /MP /MDd)
    target_link_options(SurgeFlappyBird PUBLIC /DEBUG:FULL)
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(SurgeFlappyBird PUBLIC debuginfod)
  endif()

endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  if(SURGE_COMPILER_FLAG_STYLE MATCHES "msvc")
  target_compile_options(SurgeFlappyBird PUBLIC /MP /MD)
  endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Profile" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  target_link_libraries(SurgeFlappyBird PRIVATE Tracy::TracyClient)
  
  if(SURGE_COMPILER_FLAG_STYLE MATCHES "gcc")
    target_compile_options(SurgeFlappyBird PUBLIC -g3 -ggdb3 -fno-omit-frame-pointer)
    target_link_options(SurgeFlappyBird PUBLIC -g3 -ggdb3 -fno-omit-frame-pointer -rdynamic)
  else()
    target_compile_options(SurgeFlappyBird PUBLIC /MP /MD)
    target_link_options(SurgeFlappyBird PUBLIC /DEBUG:FULL)
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(SurgeFlappyBird PUBLIC debuginfod)
    target_link_libraries(SurgeFlappyBird PUBLIC unwind)
  endif()
endif()

# -----------------------------------------
# Link and build order dependencies
# -----------------------------------------
target_link_libraries(SurgeFlappyBird PUBLIC SurgeCore)