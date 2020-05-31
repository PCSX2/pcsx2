# This file is used to be invoked at build time. It generates the needed
# resource XML file.

# Input variables that need to provided when invoking this script:
# GXML_OUTPUT             The output file path where to save the XML file.
# GXML_COMPRESS_ALL       Sets all COMPRESS flags in all resources in resource
#                         list.
# GXML_NO_COMPRESS_ALL    Removes all COMPRESS flags in all resources in
#                         resource list.
# GXML_STRIPBLANKS_ALL    Sets all STRIPBLANKS flags in all resources in
#                         resource list.
# GXML_NO_STRIPBLANKS_ALL Removes all STRIPBLANKS flags in all resources in
#                         resource list.
# GXML_TOPIXDATA_ALL      Sets all TOPIXDATA flags i nall resources in resource
#                         list.
# GXML_NO_TOPIXDATA_ALL   Removes all TOPIXDATA flags in all resources in
#                         resource list.
# GXML_PREFIX             Overrides the resource prefix that is prepended to
#                         each relative name in registered resources.
# GXML_RESOURCES          The list of resource files. Whether absolute or
#                         relative path is equal.

# Include the GENERATE_GXML() function.
include(${CMAKE_CURRENT_LIST_DIR}/GenerateGXML.cmake)

# Set flags to actual invocation flags.
if(GXML_COMPRESS_ALL)
    set(GXML_COMPRESS_ALL COMPRESS_ALL)
endif()
if(GXML_NO_COMPRESS_ALL)
    set(GXML_NO_COMPRESS_ALL NO_COMPRESS_ALL)
endif()
if(GXML_STRIPBLANKS_ALL)
    set(GXML_STRIPBLANKS_ALL STRIPBLANKS_ALL)
endif()
if(GXML_NO_STRIPBLANKS_ALL)
    set(GXML_NO_STRIPBLANKS_ALL NO_STRIPBLANKS_ALL)
endif()
if(GXML_TOPIXDATA_ALL)
    set(GXML_TOPIXDATA_ALL TOPIXDATA_ALL)
endif()
if(GXML_NO_TOPIXDATA_ALL)
    set(GXML_NO_TOPIXDATA_ALL NO_TOPIXDATA_ALL)
endif()

# Replace " " with ";" to import the list over the command line. Otherwise
# CMake would interprete the passed resources as a whole string.
string(REPLACE " " ";" GXML_RESOURCES ${GXML_RESOURCES})

# Invoke the gresource XML generation function.
generate_gxml(${GXML_OUTPUT}
              ${GXML_COMPRESS_ALL} ${GXML_NO_COMPRESS_ALL}
              ${GXML_STRIPBLANKS_ALL} ${GXML_NO_STRIPBLANKS_ALL}
              ${GXML_TOPIXDATA_ALL} ${GXML_NO_TOPIXDATA_ALL}
              PREFIX ${GXML_PREFIX}
              RESOURCES ${GXML_RESOURCES})

