// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebuggerWidget.h"

#include "JsonValueWrapper.h"

#include "DebugTools/DebugInterface.h"

#include "common/Assertions.h"

DebugInterface& DebuggerWidget::cpu() const
{
	if (m_cpu_override.has_value())
		return DebugInterface::get(*m_cpu_override);

	pxAssertRel(m_cpu, "DebuggerWidget::cpu called on object with null cpu.");
	return *m_cpu;
}

bool DebuggerWidget::setCpu(DebugInterface* new_cpu)
{
	BreakPointCpu before = cpu().getCpuType();
	m_cpu = new_cpu;
	BreakPointCpu after = cpu().getCpuType();
	return before == after;
}

std::optional<BreakPointCpu> DebuggerWidget::cpuOverride() const
{
	return m_cpu_override;
}

bool DebuggerWidget::setCpuOverride(std::optional<BreakPointCpu> new_cpu)
{
	BreakPointCpu before = cpu().getCpuType();
	m_cpu_override = new_cpu;
	BreakPointCpu after = cpu().getCpuType();
	return before == after;
}

void DebuggerWidget::toJson(JsonValueWrapper& json)
{
	rapidjson::Value cpu_name;
	if (m_cpu_override)
	{
		switch (*m_cpu_override)
		{
			case BREAKPOINT_EE:
				cpu_name.SetString("EE");
				break;
			case BREAKPOINT_IOP:
				cpu_name.SetString("IOP");
				break;
			default:
				return;
		}
	}

	json.value().AddMember("target", cpu_name, json.allocator());
}

void DebuggerWidget::fromJson(JsonValueWrapper& json)
{
}

void DebuggerWidget::applyMonospaceFont()
{
	// Easiest way to handle cross platform monospace fonts
	// There are issues related to TabWidget -> Children font inheritance otherwise
#if defined(WIN32)
	setStyleSheet(QStringLiteral("font: 10pt 'Lucida Console'"));
#elif defined(__APPLE__)
	setStyleSheet(QStringLiteral("font: 10pt 'Monaco'"));
#else
	setStyleSheet(QStringLiteral("font: 10pt 'Monospace'"));
#endif
}

DebuggerWidget::DebuggerWidget(DebugInterface* cpu, QWidget* parent)
	: QWidget(parent)
	, m_cpu(cpu)
{
}
