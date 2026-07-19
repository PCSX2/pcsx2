// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// Q_DECLARE_METATYPE specializations shared by every moc'd header that mentions
// these types in a signal/slot. They must appear in a common header (not just
// QtHost.h): AUTOMOC compiles all moc_*.cpp files into one mocs_compilation.cpp
// TU, and a moc that instantiates QMetaTypeId<T> before another header's
// specialization is an "explicit specialization after instantiation" error —
// which moc comes first depends on the autogen bucket layout, so it can flip
// whenever the source list changes.

#include <memory>
#include <optional>

#include <QtCore/QMetaType>

#include "pcsx2/Config.h"
#include "pcsx2/Input/InputManager.h"

struct VMBootParameters;

enum class CDVD_SourceType : uint8_t;

namespace Achievements
{
	enum class LoginRequestReason;
}

Q_DECLARE_METATYPE(std::shared_ptr<VMBootParameters>);
Q_DECLARE_METATYPE(std::optional<bool>);
Q_DECLARE_METATYPE(GSRendererType);
Q_DECLARE_METATYPE(InputBindingKey);
Q_DECLARE_METATYPE(CDVD_SourceType);
Q_DECLARE_METATYPE(Achievements::LoginRequestReason);
