###########################################################################
# Copyright 2015, 2016, 2017 IoT.bzh
#
# author: Fulup Ar Foll <fulup@iot.bzh>
# contrib: Romain Forlot <romain.forlot@iot.bzh>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################


#--------------------------------------------------------------------------
#  WARNING:
#     Do not change this cmake template
#     Customise your preferences in "./conf.d/cmake/config.cmake"
#--------------------------------------------------------------------------

file(GLOB project_cmakefiles ${PROJECT_APP_TEMPLATES_DIR}/cmake/cmake.d/[0-9][0-9]-*.cmake)
list(SORT project_cmakefiles)
file(GLOB home_cmakefiles $ENV{HOME}/.config/app-templates/cmake.d/[0-9][0-9]-common*.cmake $ENV{HOME}/.config/app-templates/cmake.d/[0-9][0-9]-${PROJECT_NAME}*.cmake)
list(SORT home_cmakefiles)
file(GLOB system_cmakefiles /etc/app-templates/cmake.d/[0-9][0-9]-common*.cmake /etc/app-templates/cmake.d/[0-9][0-9]-${PROJECT_NAME}*.cmake)
list(SORT system_cmakefiles)

foreach(file ${system_cmakefiles} ${home_cmakefiles} ${project_cmakefiles})
	message(STATUS "Include: ${file}")
	include(${file})
endforeach()

file(GLOB project_cmakefiles ${ENTRY_POINT}/cmake/[0-9][0-9]-${OSRELEASE}*.cmake )
list(SORT project_cmakefiles)
if(NOT project_cmakefiles)
	file(GLOB project_cmakefiles ${ENTRY_POINT}/cmake/[0-9][0-9]-default*.cmake)
endif()

foreach(file ${project_cmakefiles})
	message(STATUS "Include: ${file}")
	include(${file})
endforeach()

if(DEFINED PROJECT_SRC_DIR_PATTERN)
	project_subdirs_add(${PROJECT_SRC_DIR_PATTERN})
else()
	project_subdirs_add()
endif(DEFINED PROJECT_SRC_DIR_PATTERN)

configure_files_in_dir(${PROJECT_APP_TEMPLATES_DIR}/${ENTRY_POINT}/template.d)
configure_files_in_dir($ENV{HOME}/.config/app-templates/scripts)
configure_files_in_dir(/etc/app-templates/scripts)

project_targets_populate()
remote_targets_populate()
project_package_build()
project_closing_msg()
