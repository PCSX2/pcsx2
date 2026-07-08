# This file is included from the top-level CMakeLists.txt.  We just store it
# here to avoid cluttering up that file.

set(PKGNAME ${CMAKE_PROJECT_NAME} CACHE STRING
  "Distribution package name (default: ${CMAKE_PROJECT_NAME})")
set(PKGVENDOR "The ${CMAKE_PROJECT_NAME} Project" CACHE STRING
  "Vendor name to be included in distribution package descriptions (default: The ${CMAKE_PROJECT_NAME} Project)")
set(PKGURL "https://${CMAKE_PROJECT_NAME}.org" CACHE STRING
  "URL of project web site to be included in distribution package descriptions (default: https://${CMAKE_PROJECT_NAME}.org)")
set(PKGEMAIL "information@${CMAKE_PROJECT_NAME}.org" CACHE STRING
  "E-mail of project maintainer to be included in distribution package descriptions (default: information@${CMAKE_PROJECT_NAME}.org")
set(PKGID "com.${CMAKE_PROJECT_NAME}.${PKGNAME}" CACHE STRING
  "Globally unique package identifier (reverse DNS notation) (default: com.${CMAKE_PROJECT_NAME}.${PKGNAME})")
