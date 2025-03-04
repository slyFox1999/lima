# Copyright 2021 CEA LIST
# SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.10)

project(
  DeepLima
  VERSION 0.1
  DESCRIPTION "DNN-base implementation of NL parsing"
  LANGUAGES CXX
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../SetCompilerFlags.cmake)

# set(CMAKE_CXX_STANDARD 14)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

message("${CMAKE_PREFIX_PATH}")
message("${CMAKE_SYSTEM_PREFIX_PATH}")

set(Boost_USE_STATIC_LIBS   ON)
find_package(Boost         1.46  REQUIRED COMPONENTS program_options filesystem)
find_package(ICU           60.0  REQUIRED COMPONENTS uc)
find_package(Eigen3        3.3   REQUIRED NO_MODULE)
if(WIN32)
include_directories("${EIGEN3_INCLUDE_DIR}/..")
message("Eigen3 include: ${EIGEN3_INCLUDE_DIR}/..")
message("Eigen3 root: ${EIGEN3_USE_FILE}")
endif()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(DEEPLIMA_STANDALONE ON)
  find_package(Torch REQUIRED)
  add_subdirectory(../extern "${CMAKE_BINARY_DIR}/extern")
  include_directories(../extern)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

include_directories(AFTER "include")
include_directories(AFTER "libs")

add_subdirectory(libs)
add_subdirectory(apps)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  add_subdirectory(tests)
endif()
