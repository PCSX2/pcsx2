#include "c4/yml/version.hpp"

namespace c4 {
namespace yml {

csubstr version()
{
  return RYML_VERSION;
}

int version_major()
{
  return RYML_VERSION_MAJOR;
}

int version_minor()
{
  return RYML_VERSION_MINOR;
}

int version_patch()
{
  return RYML_VERSION_PATCH;
}

} // namespace yml
} // namespace c4
