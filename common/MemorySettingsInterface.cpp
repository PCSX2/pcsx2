/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/MemorySettingsInterface.h"
#include "common/StringUtil.h"

MemorySettingsInterface::MemorySettingsInterface() = default;

MemorySettingsInterface::~MemorySettingsInterface() = default;

bool MemorySettingsInterface::Save()
{
	return false;
}

void MemorySettingsInterface::Clear()
{
	m_sections.clear();
}

bool MemorySettingsInterface::GetIntValue(const char* section, const char* key, s32* value) const
{
	const auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		return false;

	const auto iter = sit->second.find(key);
	if (iter == sit->second.end())
		return false;

	std::optional<s32> parsed = StringUtil::FromChars<s32>(iter->second, 10);
	if (!parsed.has_value())
		return false;

	*value = parsed.value();
	return true;
}

bool MemorySettingsInterface::GetUIntValue(const char* section, const char* key, u32* value) const
{
	const auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		return false;

	const auto iter = sit->second.find(key);
	if (iter == sit->second.end())
		return false;

	std::optional<u32> parsed = StringUtil::FromChars<u32>(iter->second, 10);
	if (!parsed.has_value())
		return false;

	*value = parsed.value();
	return true;
}

bool MemorySettingsInterface::GetFloatValue(const char* section, const char* key, float* value) const
{
	const auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		return false;

	const auto iter = sit->second.find(key);
	if (iter == sit->second.end())
		return false;

	std::optional<float> parsed = StringUtil::FromChars<float>(iter->second);
	if (!parsed.has_value())
		return false;

	*value = parsed.value();
	return true;
}

bool MemorySettingsInterface::GetDoubleValue(const char* section, const char* key, double* value) const
{
	const auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		return false;

	const auto iter = sit->second.find(key);
	if (iter == sit->second.end())
		return false;

	std::optional<double> parsed = StringUtil::FromChars<double>(iter->second);
	if (!parsed.has_value())
		return false;

	*value = parsed.value();
	return true;
}

bool MemorySettingsInterface::GetBoolValue(const char* section, const char* key, bool* value) const
{
	const auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		return false;

	const auto iter = sit->second.find(key);
	if (iter == sit->second.end())
		return false;

	std::optional<bool> parsed = StringUtil::FromChars<bool>(iter->second);
	if (!parsed.has_value())
		return false;

	*value = parsed.value();
	return true;
}

bool MemorySettingsInterface::GetStringValue(const char* section, const char* key, std::string* value) const
{
	const auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		return false;

	const auto iter = sit->second.find(key);
	if (iter == sit->second.end())
		return false;

	*value = iter->second;
	return true;
}

void MemorySettingsInterface::SetIntValue(const char* section, const char* key, s32 value)
{
	SetValue(section, key, std::to_string(value));
}

void MemorySettingsInterface::SetUIntValue(const char* section, const char* key, u32 value)
{
	SetValue(section, key, std::to_string(value));
}

void MemorySettingsInterface::SetFloatValue(const char* section, const char* key, float value)
{
	SetValue(section, key, std::to_string(value));
}

void MemorySettingsInterface::SetDoubleValue(const char* section, const char* key, double value)
{
	SetValue(section, key, std::to_string(value));
}

void MemorySettingsInterface::SetBoolValue(const char* section, const char* key, bool value)
{
	SetValue(section, key, std::to_string(value));
}

void MemorySettingsInterface::SetStringValue(const char* section, const char* key, const char* value)
{
	SetValue(section, key, value);
}

std::vector<std::pair<std::string, std::string>> MemorySettingsInterface::GetKeyValueList(const char* section) const
{
	std::vector<std::pair<std::string, std::string>> output;
	auto sit = m_sections.find(section);
	if (sit != m_sections.end())
	{
		for (const auto& it : sit->second)
			output.emplace_back(it.first, it.second);
	}
	return output;
}

void MemorySettingsInterface::SetKeyValueList(const char* section, const std::vector<std::pair<std::string, std::string>>& items)
{
	auto sit = m_sections.find(section);
	sit->second.clear();
	for (const auto& [key, value] : items)
		sit->second.emplace(key, value);
}

void MemorySettingsInterface::SetValue(const char* section, const char* key, std::string value)
{
	auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		sit = m_sections.emplace(std::make_pair(std::string(section), KeyMap())).first;

	const auto range = sit->second.equal_range(key);
	if (range.first == sit->second.end())
	{
		sit->second.emplace(std::string(key), std::move(value));
		return;
	}

	auto iter = range.first;
	iter->second = std::move(value);
	++iter;

	// remove other values
	while (iter != range.second)
	{
		auto remove = iter++;
		sit->second.erase(remove);
	}
}

std::vector<std::string> MemorySettingsInterface::GetStringList(const char* section, const char* key) const
{
	std::vector<std::string> ret;

	const auto sit = m_sections.find(section);
	if (sit != m_sections.end())
	{
		const auto range = sit->second.equal_range(key);
		for (auto iter = range.first; iter != range.second; ++iter)
			ret.emplace_back(iter->second);
	}

	return ret;
}

void MemorySettingsInterface::SetStringList(const char* section, const char* key, const std::vector<std::string>& items)
{
	auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		sit = m_sections.emplace(std::make_pair(std::string(section), KeyMap())).first;

	const auto range = sit->second.equal_range(key);
	for (auto iter = range.first; iter != range.second;)
		sit->second.erase(iter++);

	std::string_view keysv(key);
	for (const std::string& value : items)
		sit->second.emplace(keysv, value);
}

bool MemorySettingsInterface::RemoveFromStringList(const char* section, const char* key, const char* item)
{
	auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		sit = m_sections.emplace(std::make_pair(std::string(section), KeyMap())).first;

	const auto range = sit->second.equal_range(key);
	bool result = false;
	for (auto iter = range.first; iter != range.second;)
	{
		if (iter->second == item)
		{
			sit->second.erase(iter++);
			result = true;
		}
		else
		{
			++iter;
		}
	}

	return result;
}

bool MemorySettingsInterface::AddToStringList(const char* section, const char* key, const char* item)
{
	auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		sit = m_sections.emplace(std::make_pair(std::string(section), KeyMap())).first;

	const auto range = sit->second.equal_range(key);
	for (auto iter = range.first; iter != range.second; ++iter)
	{
		if (iter->second == item)
			return false;
	}

	sit->second.emplace(std::string(key), std::string(item));
	return true;
}

bool MemorySettingsInterface::ContainsValue(const char* section, const char* key) const
{
	const auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		return false;

	return (sit->second.find(key) != sit->second.end());
}

void MemorySettingsInterface::DeleteValue(const char* section, const char* key)
{
	auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		return;

	const auto range = sit->second.equal_range(key);
	for (auto iter = range.first; iter != range.second;)
		sit->second.erase(iter++);
}

void MemorySettingsInterface::ClearSection(const char* section)
{
	auto sit = m_sections.find(section);
	if (sit == m_sections.end())
		return;

	m_sections.erase(sit);
}
