// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "LayeredSettingsInterface.h"

#include "common/Assertions.h"

#include <unordered_set>

LayeredSettingsInterface::LayeredSettingsInterface() = default;

LayeredSettingsInterface::~LayeredSettingsInterface() = default;

bool LayeredSettingsInterface::Save(Error* error)
{
	pxFailRel("Attempting to save layered settings interface");
	return false;
}

void LayeredSettingsInterface::Clear()
{
	pxFailRel("Attempting to clear layered settings interface");
}

bool LayeredSettingsInterface::IsEmpty()
{
	return false;
}

bool LayeredSettingsInterface::GetIntValue(const char* section, const char* key, int* value) const
{
	for (u32 layer = FIRST_LAYER; layer <= LAST_LAYER; layer++)
	{
		if (SettingsInterface* sif = m_layers[layer])
		{
			if (sif->GetIntValue(section, key, value))
				return true;
		}
	}

	return false;
}

bool LayeredSettingsInterface::GetUIntValue(const char* section, const char* key, uint* value) const
{
	for (u32 layer = FIRST_LAYER; layer <= LAST_LAYER; layer++)
	{
		if (SettingsInterface* sif = m_layers[layer])
		{
			if (sif->GetUIntValue(section, key, value))
				return true;
		}
	}

	return false;
}

bool LayeredSettingsInterface::GetFloatValue(const char* section, const char* key, float* value) const
{
	for (u32 layer = FIRST_LAYER; layer <= LAST_LAYER; layer++)
	{
		if (SettingsInterface* sif = m_layers[layer])
		{
			if (sif->GetFloatValue(section, key, value))
				return true;
		}
	}

	return false;
}

bool LayeredSettingsInterface::GetDoubleValue(const char* section, const char* key, double* value) const
{
	for (u32 layer = FIRST_LAYER; layer <= LAST_LAYER; layer++)
	{
		if (SettingsInterface* sif = m_layers[layer])
		{
			if (sif->GetDoubleValue(section, key, value))
				return true;
		}
	}

	return false;
}

bool LayeredSettingsInterface::GetBoolValue(const char* section, const char* key, bool* value) const
{
	for (u32 layer = FIRST_LAYER; layer <= LAST_LAYER; layer++)
	{
		if (SettingsInterface* sif = m_layers[layer])
		{
			if (sif->GetBoolValue(section, key, value))
				return true;
		}
	}

	return false;
}

bool LayeredSettingsInterface::GetStringValue(const char* section, const char* key, std::string* value) const
{
	for (u32 layer = FIRST_LAYER; layer <= LAST_LAYER; layer++)
	{
		if (SettingsInterface* sif = m_layers[layer])
		{
			if (sif->GetStringValue(section, key, value))
				return true;
		}
	}

	return false;
}

bool LayeredSettingsInterface::GetStringValue(const char* section, const char* key, SmallStringBase* value) const
{
	for (u32 layer = FIRST_LAYER; layer <= LAST_LAYER; layer++)
	{
		if (SettingsInterface* sif = m_layers[layer]; sif != nullptr)
		{
			if (sif->GetStringValue(section, key, value))
				return true;
		}
	}

	return false;
}

void LayeredSettingsInterface::SetIntValue(const char* section, const char* key, int value)
{
	pxFailRel("Attempt to call SetIntValue() on layered settings interface");
}

void LayeredSettingsInterface::SetUIntValue(const char* section, const char* key, uint value)
{
	pxFailRel("Attempt to call SetUIntValue() on layered settings interface");
}

void LayeredSettingsInterface::SetFloatValue(const char* section, const char* key, float value)
{
	pxFailRel("Attempt to call SetFloatValue() on layered settings interface");
}

void LayeredSettingsInterface::SetDoubleValue(const char* section, const char* key, double value)
{
	pxFailRel("Attempt to call SetDoubleValue() on layered settings interface");
}

void LayeredSettingsInterface::SetBoolValue(const char* section, const char* key, bool value)
{
	pxFailRel("Attempt to call SetBoolValue() on layered settings interface");
}

void LayeredSettingsInterface::SetStringValue(const char* section, const char* key, const char* value)
{
	pxFailRel("Attempt to call SetStringValue() on layered settings interface");
}

bool LayeredSettingsInterface::ContainsValue(const char* section, const char* key) const
{
	for (u32 layer = FIRST_LAYER; layer <= LAST_LAYER; layer++)
	{
		if (SettingsInterface* sif = m_layers[layer])
		{
			if (sif->ContainsValue(section, key))
				return true;
		}
	}
	return false;
}

void LayeredSettingsInterface::DeleteValue(const char* section, const char* key)
{
	pxFailRel("Attempt to call DeleteValue() on layered settings interface");
}

void LayeredSettingsInterface::ClearSection(const char* section)
{
	pxFailRel("Attempt to call ClearSection() on layered settings interface");
}

void LayeredSettingsInterface::RemoveSection(const char* section)
{
	pxFailRel("Attempt to call RemoveSection() on layered settings interface");
}

void LayeredSettingsInterface::RemoveEmptySections()
{
	pxFailRel("Attempt to call RemoveEmptySections() on layered settings interface");
}

std::vector<std::string> LayeredSettingsInterface::GetStringList(const char* section, const char* key) const
{
	std::vector<std::string> ret;

	for (u32 layer = FIRST_LAYER; layer <= LAST_LAYER; layer++)
	{
		if (SettingsInterface* sif = m_layers[layer])
		{
			ret = sif->GetStringList(section, key);
			if (!ret.empty())
				break;
		}
	}

	return ret;
}

void LayeredSettingsInterface::SetStringList(const char* section, const char* key, const std::vector<std::string>& items)
{
	pxFailRel("Attempt to call SetStringList() on layered settings interface");
}

bool LayeredSettingsInterface::RemoveFromStringList(const char* section, const char* key, const char* item)
{
	pxFailRel("Attempt to call RemoveFromStringList() on layered settings interface");
	return false;
}

bool LayeredSettingsInterface::AddToStringList(const char* section, const char* key, const char* item)
{
	pxFailRel("Attempt to call AddToStringList() on layered settings interface");
	return true;
}

std::vector<std::pair<std::string, std::string>> LayeredSettingsInterface::GetKeyValueList(const char* section) const
{
	std::unordered_set<std::string_view> seen;
	std::vector<std::pair<std::string, std::string>> ret;
	for (u32 layer = FIRST_LAYER; layer <= LAST_LAYER; layer++)
	{
		if (SettingsInterface* sif = m_layers[layer])
		{
			const size_t newly_added_begin = ret.size();
			std::vector<std::pair<std::string, std::string>> entries = sif->GetKeyValueList(section);
			for (std::pair<std::string, std::string>& entry : entries)
			{
				if (seen.find(entry.first) != seen.end())
					continue;
				ret.push_back(std::move(entry));
			}
			// Mark keys as seen after processing all entries in case the layer has multiple entries for a specific key
			for (auto cur = ret.begin() + newly_added_begin, end = ret.end(); cur < end; cur++)
				seen.insert(cur->first);
		}
	}
	return ret;
}

void LayeredSettingsInterface::SetKeyValueList(const char* section, const std::vector<std::pair<std::string, std::string>>& items)
{
	pxFailRel("Attempt to call SetKeyValueList() on layered settings interface");
}
