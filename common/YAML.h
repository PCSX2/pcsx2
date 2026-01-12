// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Error.h"

#include "ryml_std.hpp"
#include "ryml.hpp"
#include "ryml.hpp"

#include <optional>

/// Parse a YAML file with RapidYAML, and use setjmp/longjmp to recover from
/// parsing errors (as is recommended by the documentation for cases where
/// exceptions are disabled). The file_name parameter is only used for error
/// messages, which are returned via the error parameter.
std::optional<ryml::Tree> ParseYAMLFromString(ryml::csubstr yaml, ryml::csubstr file_name, Error* error);
