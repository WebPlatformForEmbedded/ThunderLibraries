# If not stated otherwise in this file or this component's license file the
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

option(BLUETOOTH
        "Include the bluetooth library." OFF)
option(BROADCAST
        "Include the broadcasting library." OFF)

#
# Build type specific options
#
if(NOT BUILD_TYPE)
    set(BUILD_TYPE Production)
    message(AUTHOR_WARNING "BUILD_TYPE not set, assuming '${BUILD_TYPE}'")
endif()
if("${BUILD_TYPE}" STREQUAL "Debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
elseif("${BUILD_TYPE}" STREQUAL "DebugOptimized")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
elseif("${BUILD_TYPE}" STREQUAL "ReleaseSymbols")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
elseif("${BUILD_TYPE}" STREQUAL "Release")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
elseif("${BUILD_TYPE}" STREQUAL "Production")
    set(CMAKE_BUILD_TYPE "MinSizeRel" CACHE STRING "Choose the type of build." FORCE)
else()
    message(FATAL_ERROR "Invalid BUILD_TYPE: '${BUILD_TYPE}'")
endif()

if(BLUETOOTH)
    add_subdirectory(bluetooth)
endif()

if(BROADCAST)
    add_subdirectory(broadcast)
endif()
