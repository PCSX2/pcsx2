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

bool DebuggerWidget::setCpu(DebugInterface& new_cpu)
{
	BreakPointCpu before = cpu().getCpuType();
	m_cpu = &new_cpu;
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
	if (m_cpu_override.has_value())
	{
		const char* cpu_name = DebugInterface::cpuName(*m_cpu_override);

		rapidjson::Value target;
		target.SetString(cpu_name, strlen(cpu_name));
		json.value().AddMember("target", target, json.allocator());
	}
}

bool DebuggerWidget::fromJson(JsonValueWrapper& json)
{
	auto target = json.value().FindMember("target");
	if (target != json.value().MemberEnd() && target->value.IsString())
	{
		for (BreakPointCpu cpu : DEBUG_CPUS)
			if (strcmp(DebugInterface::cpuName(cpu), target->value.GetString()) == 0)
				m_cpu_override = cpu;
	}

	return true;
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
