// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QString>

#include "DebugTools/ccc/ast.h"

// Take a string e.g. "int*[3]" and generates an AST. Supports type names by
// themselves as well as pointers, references and arrays. Pointer characters
// appear in the same order as they would in C source code, however array
// subscripts appear in the opposite order, so that it is possible to specify a
// pointer to an array.
std::unique_ptr<ccc::ast::Node> stringToType(std::string_view string, const ccc::SymbolDatabase& database, QString& error_out);

// Opposite of stringToType. Takes an AST node and converts it to a string.
QString typeToString(const ccc::ast::Node* type, const ccc::SymbolDatabase& database);
