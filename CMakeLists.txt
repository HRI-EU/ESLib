################################################################################
#
#  Copyright (c) 2020, Honda Research Institute Europe GmbH.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#
#  3. Neither the name of the copyright holder nor the names of its
#     contributors may be used to endorse or promote products derived from
#     this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY EXPRESS OR
#  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
#  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
#  IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
#  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
#  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
################################################################################
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
SET(BST_INSTALL_CATEGORY Libraries) 

PROJECT(EventSystem)
SET(${PROJECT_NAME}_VERSION 1.0)

# Useful CMake modules
INCLUDE(GNUInstallDirs)
INCLUDE(CMakePackageConfigHelpers)

# Check if EventSystem is being used directly or via add_subdirectory
SET(EventSystem_MASTER_PROJECT OFF)
IF(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  SET(EventSystem_MASTER_PROJECT ON)
  IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    SET(CMAKE_INSTALL_PREFIX "$ENV{SIT}/Libraries/ESLib/1.0" CACHE PATH "Default sand box directory" FORCE)
  ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
ENDIF()

# Unfortunately, cmake 2.8 doesn't support interface targets. We use a variable instead.
SET(EventSystem_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

# This also means that we can't force C++11 effectively - do it the old-fashioned way. 
IF(UNIX)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ENDIF()

# Target for header-only ESlib library. Adding this to the target_link_libraries will
# automatically inherit the include path.
ADD_LIBRARY(ESlib INTERFACE)
TARGET_INCLUDE_DIRECTORIES(ESlib INTERFACE ${EventSystem_INCLUDE_DIR})

# A test executable
ADD_EXECUTABLE(TestEventSystem 
  examples/main.cpp
  examples/test_subscriptiononly.cpp)
TARGET_INCLUDE_DIRECTORIES(TestEventSystem PRIVATE ${EventSystem_INCLUDE_DIR})

# Generate documentation
IF (EventSystem_MASTER_PROJECT)
  ADD_SUBDIRECTORY(doc)
ENDIF()

# Add validation to cmake path
LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/utils/cmake)

# Installation of headers & co - do this even if we're a subproject.

# Install header files
INSTALL(DIRECTORY ${${PROJECT_NAME}_INCLUDE_DIR}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ESLib)

# Install validation files
SET(VALIDATION_INSTALL_DIR share/ESLibValidation)
INSTALL(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/utils/ DESTINATION ${VALIDATION_INSTALL_DIR})

# Generate package config/version
CONFIGURE_PACKAGE_CONFIG_FILE(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION share/cmake/${PROJECT_NAME}
                              PATH_VARS VALIDATION_INSTALL_DIR CMAKE_INSTALL_INCLUDEDIR)

WRITE_BASIC_PACKAGE_VERSION_FILE(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                 VERSION ${${PROJECT_NAME}_VERSION}
                                 COMPATIBILITY AnyNewerVersion)

# Install config files
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
        DESTINATION share/cmake/${PROJECT_NAME})
