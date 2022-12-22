# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2022 Metrological
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

find_package(WPEFramework)

find_package(${NAMESPACE}Definitions REQUIRED)
find_package(${NAMESPACE}Core REQUIRED)
find_package(CompileSettingsDebug REQUIRED)

add_executable(cb-server Server.cpp)

target_link_libraries(cb-server PRIVATE
    CompileSettingsDebug::CompileSettingsDebug
    ${NAMESPACE}Definitions::${NAMESPACE}Definitions
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${NAMESPACE}CompositorBuffer::${NAMESPACE}CompositorBuffer)

install(TARGETS cb-server DESTINATION bin)


add_executable(cb-client Client.cpp)

target_link_libraries(cb-client PRIVATE
    CompileSettingsDebug::CompileSettingsDebug
    ${NAMESPACE}Definitions::${NAMESPACE}Definitions
    ${NAMESPACE}Core::${NAMESPACE}Core
    ${NAMESPACE}CompositorBuffer::${NAMESPACE}CompositorBuffer)

install(TARGETS cb-client DESTINATION bin)



