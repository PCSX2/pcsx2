find_package(Java REQUIRED)

# Allow the Java compiler flags to be set using an environment variable
if(NOT DEFINED CMAKE_JAVA_COMPILE_FLAGS AND DEFINED ENV{JAVAFLAGS})
  set(CMAKE_JAVA_COMPILE_FLAGS $ENV{JAVAFLAGS})
endif()

include(UseJava)

set(CMAKE_JAVA_COMPILE_FLAGS "${CMAKE_JAVA_COMPILE_FLAGS} -J-Dfile.encoding=UTF8")
message(STATUS "CMAKE_JAVA_COMPILE_FLAGS = ${CMAKE_JAVA_COMPILE_FLAGS}")
string(REGEX REPLACE " " ";" CMAKE_JAVA_COMPILE_FLAGS "${CMAKE_JAVA_COMPILE_FLAGS}")

set(JAVAARGS "" CACHE STRING "Additional arguments to pass to java when running unit tests (example: -d32)")
message(STATUS "JAVAARGS = ${JAVAARGS}")

set(JAVA_SOURCES TJ.java
  TJUnitTest.java
  TJComp.java
  TJDecomp.java
  TJTran.java
  TJBench.java)

if(MSYS)
  # UGLY HACK ALERT: If we don't do this, then UseJava.cmake will separate
  # class path members with a semicolon, which is interpreted as a command
  # separator by the MSYS shell.
  set(CMAKE_HOST_SYSTEM_NAME_BAK ${CMAKE_HOST_SYSTEM_NAME})
  set(CMAKE_HOST_SYSTEM_NAME "MSYS")
endif()
add_jar(turbojpeg-jna ${JAVA_SOURCES} OUTPUT_NAME turbojpeg-jna
  INCLUDE_JARS "${WITH_JNA}" ENTRY_POINT TJBench)
if(MSYS)
  set(CMAKE_HOST_SYSTEM_NAME ${CMAKE_HOST_SYSTEM_NAME_BAK})
endif()

if(NOT DEFINED CMAKE_INSTALL_DEFAULT_JAVADIR)
  set(CMAKE_INSTALL_DEFAULT_JAVADIR "<CMAKE_INSTALL_DATAROOTDIR>/java")
endif()
GNUInstallDirs_set_install_dir(JAVADIR
  "The directory into which Java classes should be installed")
GNUInstallDirs_get_absolute_install_dir(CMAKE_INSTALL_FULL_JAVADIR
  CMAKE_INSTALL_JAVADIR)
set(CMAKE_INSTALL_JAVADIR ${CMAKE_INSTALL_JAVADIR} PARENT_SCOPE)
set(CMAKE_INSTALL_FULL_JAVADIR ${CMAKE_INSTALL_FULL_JAVADIR} PARENT_SCOPE)
if(NOT CMAKE_INSTALL_JAVADIR)
  message(FATAL_ERROR "CMAKE_INSTALL_JAVADIR cannot be blank")
endif()
report_directory(JAVADIR)
install_jar(turbojpeg-jna DESTINATION ${CMAKE_INSTALL_JAVADIR}
  COMPONENT java)
mark_as_advanced(CLEAR CMAKE_INSTALL_JAVADIR)
