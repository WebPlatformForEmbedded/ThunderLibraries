#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2020 Metrological
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.15)
project(Libraries)

find_package(Thunder)

if (BUILD_REFERENCE)
    add_definitions (-DBUILD_REFERENCE=${BUILD_REFERENCE})
endif()

if(ENABLE_STRICT_COMPILER_SETTINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Compiling with Clang")
        set(CMAKE_STRICT_COMPILER_SETTINGS "-Weverything -Wextra -Wpedantic -Werror")
        set(CMAKE_STRICT_CXX_COMPILER_SETTINGS "${CMAKE_STRICT_COMPILER_SETTINGS} -Wnon-virtual-dtor")
    elseif(${CMAKE_COMPILER_IS_GNUCXX})
        message(VERBOSE "Compiling with GCC")
        set(CMAKE_STRICT_COMPILER_SETTINGS "-Wall -Wextra -Wpedantic -Werror")
        set(CMAKE_STRICT_CXX_COMPILER_SETTINGS "${CMAKE_STRICT_COMPILER_SETTINGS} -Wnon-virtual-dtor")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        message(VERBOSE "Compiling with MS Visual Studio")
        set(CMAKE_STRICT_COMPILER_SETTINGS "/W4")
    else()
        message(VERBOSE "Compiler ${CMAKE_CXX_COMPILER_ID}")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_STRICT_CXX_COMPILER_SETTINGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_STRICT_COMPILER_SETTINGS}")
endif()

add_subdirectory(Source)

