CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

# TODO: Proper name
PROJECT(EventSystem)
SET(${PROJECT_NAME}_VERSION 1.0)

# Useful CMake modules
INCLUDE(GNUInstallDirs)
INCLUDE(CMakePackageConfigHelpers)

# Check if EventSystem is being used directly or via add_subdirectory
SET(EventSystem_MASTER_PROJECT OFF)
IF(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  SET(EventSystem_MASTER_PROJECT ON)
ENDIF()

# Unfortunately, cmake 2.8 doesn't support interface targets. We use a variable instead.
SET(EventSystem_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

# This also means that we can't force C++11 effectively - do it the old-fashioned way
IF(UNIX)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ENDIF()
# TODO windows. If even necessary, since windows isn't stuck with old cmake/gcc like linux...

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
INSTALL(DIRECTORY ${${PROJECT_NAME}_INCLUDE_DIR}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

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
