# Install script for directory: /srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/opensubdiv/sdc

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opensubdiv/sdc" TYPE FILE PERMISSIONS OWNER_READ GROUP_READ WORLD_READ FILES
    "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/opensubdiv/sdc/bilinearScheme.h"
    "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/opensubdiv/sdc/catmarkScheme.h"
    "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/opensubdiv/sdc/crease.h"
    "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/opensubdiv/sdc/loopScheme.h"
    "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/opensubdiv/sdc/options.h"
    "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/opensubdiv/sdc/scheme.h"
    "/srv/fb2/feng0732/0092-opensubdiv-3-e6df33b6/opensubdiv/sdc/types.h"
    )
endif()

