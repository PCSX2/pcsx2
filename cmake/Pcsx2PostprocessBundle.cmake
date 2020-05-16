# This file should be run with cmake -DPCSX2_BUNDLE_PATH=path -P Pcsx2PostprocessBundle.cmake

get_filename_component(PCSX2_BUNDLE_PATH "${PCSX2_BUNDLE_PATH}" ABSOLUTE)

# Fixup plugins too
file(GLOB_RECURSE plugins "${PCSX2_BUNDLE_PATH}/Contents/MacOS/*.dylib")

include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS ON)
fixup_bundle("${PCSX2_BUNDLE_PATH}" "${plugins}" "")
