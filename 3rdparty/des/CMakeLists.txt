add_library(des
  des.cpp
  des.h
)
target_include_directories(des PUBLIC .)
add_library(PkgConfig::DES ALIAS des)