# This file is included from the top-level CMakeLists.txt.  We just store it
# here to avoid cluttering up that file.

###############################################################################
# Linux RPM and DEB
###############################################################################

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")

set(RPMARCH ${CMAKE_SYSTEM_PROCESSOR})
if(CPU_TYPE STREQUAL "x86_64")
  set(DEBARCH amd64)
elseif(CPU_TYPE STREQUAL "arm64")
  set(DEBARCH ${CPU_TYPE})
elseif(CPU_TYPE STREQUAL "arm")
  check_c_source_compiles("
    #if __ARM_PCS_VFP != 1
    #error \"float ABI != hard\"
    #endif
    int main(void) { return 0; }" HAVE_HARD_FLOAT)
  if(HAVE_HARD_FLOAT)
    set(RPMARCH armv7hl)
    set(DEBARCH armhf)
  else()
    set(RPMARCH armel)
    set(DEBARCH armel)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR_LC STREQUAL "ppc64le")
  set(DEBARCH ppc64el)
elseif(CPU_TYPE STREQUAL "powerpc" AND BITS EQUAL 32)
  set(RPMARCH ppc)
  set(DEBARCH ppc)
else()
  set(DEBARCH ${CMAKE_SYSTEM_PROCESSOR})
endif()
message(STATUS "RPM architecture = ${RPMARCH}, DEB architecture = ${DEBARCH}")

# Re-set CMAKE_POSITION_INDEPENDENT_CODE so that the RPM spec file works
# properly
boolean_number(CMAKE_POSITION_INDEPENDENT_CODE)

configure_file(release/makerpm.in pkgscripts/makerpm)
if(NOT WITH_JNA)
  set(RPM_WITH_JNA %{nil})
else()
  set(RPM_WITH_JNA ${WITH_JNA})
endif()
configure_file(release/rpm.spec.in pkgscripts/rpm.spec @ONLY)

add_custom_target(rpm pkgscripts/makerpm
  DEPENDS rdjpgcom wrjpgcom cjpeg djpeg jpegtran
  SOURCES pkgscripts/makerpm)

configure_file(release/makesrpm.in pkgscripts/makesrpm)

add_custom_target(srpm pkgscripts/makesrpm
  SOURCES pkgscripts/makesrpm
  DEPENDS dist)

configure_file(release/makedpkg.in pkgscripts/makedpkg)
configure_file(release/deb-control.in pkgscripts/deb-control)

add_custom_target(deb pkgscripts/makedpkg
  SOURCES pkgscripts/makedpkg)

endif() # Linux


###############################################################################
# Windows installer (NullSoft Installer)
###############################################################################

if(WIN32)

if(MSVC_LIKE)
  set(INST_PLATFORM "Visual C++")
  set(INST_ID vc)
  set(INST_NAME ${CMAKE_PROJECT_NAME}-${VERSION}-${INST_ID})
  set(INST_REG_NAME ${CMAKE_PROJECT_NAME})
elseif(MINGW)
  set(INST_PLATFORM GCC)
  set(INST_ID gcc)
  set(INST_NAME ${CMAKE_PROJECT_NAME}-${VERSION}-${INST_ID})
  set(INST_REG_NAME ${CMAKE_PROJECT_NAME}-${INST_ID})
  set(INST_DEFS -DGCC)
endif()

if(BITS EQUAL 64)
  set(INST_PLATFORM "${INST_PLATFORM} 64-bit")
  if(CPU_TYPE STREQUAL "arm64")
    set(INST_NAME ${INST_NAME}-arm64)
  elseif(CPU_TYPE STREQUAL "x86_64")
    set(INST_NAME ${INST_NAME}-x64)
  else()
    set(INST_NAME ${INST_NAME}64)
  endif()
  set(INST_REG_NAME ${INST_REG_NAME}64)
  set(INST_DEFS ${INST_DEFS} -DWIN64)
elseif(CPU_TYPE STREQUAL "i386")
  set(INST_NAME ${INST_NAME}-x86)
endif()

if(WITH_JNA)
  set(INST_DEFS ${INST_DEFS} -DJNA)
endif()

if(GENERATOR_IS_MULTI_CONFIG)
  set(INST_DEFS ${INST_DEFS} "-DBUILDDIR=${CMAKE_CFG_INTDIR}\\")
else()
  set(INST_DEFS ${INST_DEFS} "-DBUILDDIR=")
endif()

string(REGEX REPLACE "/" "\\\\" INST_DIR "${CMAKE_INSTALL_PREFIX}")

configure_file(release/installer.nsi.in installer.nsi @ONLY)
# TODO: It would be nice to eventually switch to CPack and eliminate this mess,
# but not today.
configure_file(win/projectTargets.cmake.in
  win/${CMAKE_PROJECT_NAME}Targets.cmake @ONLY)
configure_file(win/${INST_ID}/projectTargets-release.cmake.in
  win/${CMAKE_PROJECT_NAME}Targets-release.cmake @ONLY)

if(WITH_JNA)
  set(JNA_DEPEND turbojpeg-jna)
endif()
if(WITH_TURBOJPEG)
  set(TURBOJPEG_DEPEND turbojpeg turbojpeg-static tjbench)
endif()
add_custom_target(installer
  makensis -nocd ${INST_DEFS} installer.nsi
  DEPENDS jpeg jpeg-static rdjpgcom wrjpgcom cjpeg djpeg jpegtran
    ${JNA_DEPEND} ${TURBOJPEG_DEPEND}
  SOURCES installer.nsi)

endif() # WIN32


###############################################################################
# Mac DMG
###############################################################################

if(APPLE)

set(SECONDARY_BUILD "" CACHE PATH
  "Directory containing cross-compiled x86-64 or Armv8 (64-bit) iOS or macOS build to include in universal binaries")

set(MACOS_APP_CERT_NAME "" CACHE STRING
  "Name of the Developer ID Application certificate (in the macOS keychain) that should be used to sign the ${CMAKE_PROJECT_NAME} DMG.  Leave this blank to generate an unsigned DMG.")
set(MACOS_INST_CERT_NAME "" CACHE STRING
  "Name of the Developer ID Installer certificate (in the macOS keychain) that should be used to sign the ${CMAKE_PROJECT_NAME} installer package.  Leave this blank to generate an unsigned package.")

configure_file(release/makemacpkg.in pkgscripts/makemacpkg)
configure_file(release/Distribution.xml.in pkgscripts/Distribution.xml)
configure_file(release/Welcome.rtf.in pkgscripts/Welcome.rtf)
configure_file(release/uninstall.in pkgscripts/uninstall)

add_custom_target(dmg pkgscripts/makemacpkg
  SOURCES pkgscripts/makemacpkg)

endif() # APPLE


###############################################################################
# Generic
###############################################################################

add_custom_target(dist
  COMMAND git archive --prefix=${CMAKE_PROJECT_NAME}-${VERSION}/ HEAD |
    gzip > ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}-${VERSION}.tar.gz
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

configure_file(release/maketarball.in pkgscripts/maketarball)

add_custom_target(tarball pkgscripts/maketarball
  SOURCES pkgscripts/maketarball)

configure_file(release/libjpeg.pc.in pkgscripts/libjpeg.pc @ONLY)

if(WITH_TURBOJPEG)
  configure_file(release/libturbojpeg.pc.in pkgscripts/libturbojpeg.pc @ONLY)
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  pkgscripts/${CMAKE_PROJECT_NAME}ConfigVersion.cmake
  VERSION ${VERSION} COMPATIBILITY AnyNewerVersion)

configure_package_config_file(release/Config.cmake.in
  pkgscripts/${CMAKE_PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME})
