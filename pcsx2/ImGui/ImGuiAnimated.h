// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "common/Easing.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>

class ImAnimatedFloat
{
public:
	ImAnimatedFloat() = default;

	bool IsActive() const { return (m_current_value != m_end_value); }
	float GetCurrentValue() const { return m_current_value; }
	float GetStartValue() const { return m_start_value; }
	float GetEndValue() const { return m_end_value; }

	void Stop() { m_end_value = m_current_value; }
	void SetEndValue(float end_value) { m_end_value = end_value; }

	void Reset(float value)
	{
		m_current_value = value;
		m_start_value = value;
		m_end_value = value;
	}

	float UpdateAndGetValue()
	{
		if (m_current_value == m_end_value)
			return m_current_value;

		m_elapsed_time += ImGui::GetIO().DeltaTime;

		const float frac = std::min(0.05f + Easing::OutExpo(m_elapsed_time / m_duration), 1.0f);
		m_current_value = std::clamp(m_start_value + ((m_end_value - m_start_value) * frac),
			std::min(m_start_value, m_end_value), std::max(m_start_value, m_end_value));
		return m_current_value;
	}

	void Start(float start_value, float end_value, float duration)
	{
		m_current_value = start_value;
		m_start_value = start_value;
		m_end_value = end_value;
		m_elapsed_time = 0.0f;
		m_duration = duration;
	}

private:
	float m_current_value = 0.0f;
	float m_start_value = 0.0f;
	float m_end_value = 0.0f;
	float m_elapsed_time = 0.0f;
	float m_duration = 1.0f;
};

class ImAnimatedVec2
{
public:
	ImAnimatedVec2() = default;

	bool IsActive() const { return (m_current_value.x != m_end_value.x || m_current_value.y != m_end_value.y); }
	const ImVec2& GetCurrentValue() const { return m_current_value; }
	const ImVec2& GetStartValue() const { return m_start_value; }
	const ImVec2& GetEndValue() const { return m_end_value; }

	void Stop() { m_end_value = m_current_value; }
	void SetEndValue(const ImVec2& end_value) { m_end_value = end_value; }

	void Reset(const ImVec2& value)
	{
		m_current_value = value;
		m_start_value = value;
		m_end_value = value;
	}

	const ImVec2& UpdateAndGetValue()
	{
		if (m_current_value.x == m_end_value.x && m_current_value.y == m_end_value.y)
			return m_current_value;

		m_elapsed_time += ImGui::GetIO().DeltaTime;

		const float frac = std::min(0.05f + Easing::OutExpo(m_elapsed_time / m_duration), 1.0f);
		m_current_value = ImClamp(ImLerp(m_start_value, m_end_value, frac), ImMin(m_start_value, m_end_value),
			ImMax(m_start_value, m_end_value));
		return m_current_value;
	}

	void Start(const ImVec2& start_value, const ImVec2& end_value, float duration)
	{
		m_current_value = start_value;
		m_start_value = start_value;
		m_end_value = end_value;
		m_elapsed_time = 0.0f;
		m_duration = duration;
	}

private:
	ImVec2 m_current_value = {};
	ImVec2 m_start_value = {};
	ImVec2 m_end_value = {};
	float m_elapsed_time = 0.0f;
	float m_duration = 1.0f;
};
