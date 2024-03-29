# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2023 Metrological
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

cmake_minimum_required(VERSION 3.3)

find_package(WPEFramework)
find_package(${NAMESPACE}Core REQUIRED)
find_package(${NAMESPACE}Messaging REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

project(${NAMESPACE}BluetoothAudio)
project_version(1.0.0)
set(TARGET ${PROJECT_NAME})
message("Setup ${TARGET} v${PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(SBC REQUIRED)

file(GLOB CODEC_HEADERS codecs/*.h)

set(PUBLIC_HEADERS
    IAudioCodec.h
    IAudioContentProtection.h
    SDPSocket.h
    SDPProfile.h
    AVDTPSocket.h
    AVDTPProfile.h
    RTPSocket.h
    DataRecord.h
    Module.h
    bluetooth_audio.h
)

add_library(${TARGET} SHARED
    SDPSocket.cpp
    SDPProfile.cpp
    AVDTPSocket.cpp
    AVDTPProfile.cpp
    codecs/SBC.cpp
    Module.cpp
)

target_link_libraries(${TARGET}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Core::${NAMESPACE}Core
        ${NAMESPACE}Messaging::${NAMESPACE}Messaging
        ${SBC_LIBRARIES}
)

set_target_properties(${TARGET}
    PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        FRAMEWORK FALSE
        PUBLIC_HEADER "${PUBLIC_HEADERS}" # specify the public headers
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_include_directories(${TARGET}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../>
        $<INSTALL_INTERFACE:include/${NAMESPACE}>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/drivers"
)

target_compile_options(${TARGET}
    PRIVATE
        -Wno-psabi
        -fdiagnostics-color=always
)

#
# From Bluez >= v5.64 the mgmt_ltk_info struct is changed due to inclusive language changes.
# https://github.com/bluez/bluez/commit/b7d6a7d25628e9b521a29a5c133fcadcedeb2102
#
include(CheckStructHasMember)
check_struct_has_member("struct mgmt_ltk_info" central "../include/bluetooth/bluetooth.h;../include/bluetooth/mgmt.h" NO_INCLUSIVE_LANGUAGE LANGUAGE C)

if(${NO_INCLUSIVE_LANGUAGE})
    message(STATUS "Your version of bluez don't uses inclusive language anymore")
    target_compile_definitions(${TARGET} PUBLIC NO_INCLUSIVE_LANGUAGE)
endif()

install(
    TARGETS ${TARGET}  EXPORT ${TARGET}Targets  # for downstream dependencies
    ARCHIVE DESTINATION lib COMPONENT libs      # static lib
    LIBRARY DESTINATION lib COMPONENT libs      # shared lib
    RUNTIME DESTINATION bin COMPONENT libs      # binaries
    FRAMEWORK DESTINATION bin COMPONENT libs    # for mac
    PUBLIC_HEADER DESTINATION include/${NAMESPACE}/bluetooth/audio COMPONENT devel
    INCLUDES DESTINATION include/${NAMESPACE}   # default include path
)

install(
    FILES ${CODEC_HEADERS}
    DESTINATION include/${NAMESPACE}/bluetooth/audio/codecs
)

InstallPackageConfig(
    TARGETS ${TARGET}
    DESCRIPTION "Bluetooth audio library"
)

InstallCMakeConfig(
    TARGETS ${TARGET}
)
