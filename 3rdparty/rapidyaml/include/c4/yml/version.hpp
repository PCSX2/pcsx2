#ifndef _C4_YML_VERSION_HPP_
#define _C4_YML_VERSION_HPP_

/** @file version.hpp */

#define RYML_VERSION "0.10.0"
#define RYML_VERSION_MAJOR 0
#define RYML_VERSION_MINOR 10
#define RYML_VERSION_PATCH 0

#include <c4/substr.hpp>
#include <c4/yml/export.hpp>

namespace c4 {
namespace yml {

RYML_EXPORT csubstr version();
RYML_EXPORT int version_major();
RYML_EXPORT int version_minor();
RYML_EXPORT int version_patch();

} // namespace yml
} // namespace c4

#endif /* _C4_YML_VERSION_HPP_ */
