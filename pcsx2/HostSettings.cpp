/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "PrecompiledHeader.h"
#include "common/Assertions.h"
#include "Host.h"
#include "HostSettings.h"
#include "Frontend/LayeredSettingsInterface.h"

static std::mutex s_settings_mutex;
static LayeredSettingsInterface s_layered_settings_interface;

std::unique_lock<std::mutex> Host::GetSettingsLock()
{
	return std::unique_lock<std::mutex>(s_settings_mutex);
}

SettingsInterface* Host::GetSettingsInterface()
{
	return &s_layered_settings_interface;
}

std::string Host::GetBaseStringSettingValue(const char* section, const char* key, const char* default_value /*= ""*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->GetStringValue(section, key, default_value);
}

bool Host::GetBaseBoolSettingValue(const char* section, const char* key, bool default_value /*= false*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->GetBoolValue(section, key, default_value);
}

int Host::GetBaseIntSettingValue(const char* section, const char* key, int default_value /*= 0*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->GetIntValue(section, key, default_value);
}

uint Host::GetBaseUIntSettingValue(const char* section, const char* key, uint default_value /*= 0*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->GetUIntValue(section, key, default_value);
}

float Host::GetBaseFloatSettingValue(const char* section, const char* key, float default_value /*= 0.0f*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->GetFloatValue(section, key, default_value);
}

double Host::GetBaseDoubleSettingValue(const char* section, const char* key, double default_value /* = 0.0f */)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->GetDoubleValue(section, key, default_value);
}

std::vector<std::string> Host::GetBaseStringListSetting(const char* section, const char* key)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE)->GetStringList(section, key);
}

std::string Host::GetStringSettingValue(const char* section, const char* key, const char* default_value /*= ""*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetStringValue(section, key, default_value);
}

bool Host::GetBoolSettingValue(const char* section, const char* key, bool default_value /*= false*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetBoolValue(section, key, default_value);
}

int Host::GetIntSettingValue(const char* section, const char* key, int default_value /*= 0*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetIntValue(section, key, default_value);
}

uint Host::GetUIntSettingValue(const char* section, const char* key, uint default_value /*= 0*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetUIntValue(section, key, default_value);
}

float Host::GetFloatSettingValue(const char* section, const char* key, float default_value /*= 0.0f*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetFloatValue(section, key, default_value);
}

double Host::GetDoubleSettingValue(const char* section, const char* key, double default_value /*= 0.0f*/)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetDoubleValue(section, key, default_value);
}

std::vector<std::string> Host::GetStringListSetting(const char* section, const char* key)
{
	std::unique_lock lock(s_settings_mutex);
	return s_layered_settings_interface.GetStringList(section, key);
}

SettingsInterface* Host::Internal::GetBaseSettingsLayer()
{
	return s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE);
}

void Host::Internal::SetBaseSettingsLayer(SettingsInterface* sif)
{
	pxAssertRel(s_layered_settings_interface.GetLayer(LayeredSettingsInterface::LAYER_BASE) == nullptr, "Base layer has not been set");
	s_layered_settings_interface.SetLayer(LayeredSettingsInterface::LAYER_BASE, sif);
}

void Host::Internal::SetGameSettingsLayer(SettingsInterface* sif)
{
	std::unique_lock lock(s_settings_mutex);
	s_layered_settings_interface.SetLayer(LayeredSettingsInterface::LAYER_GAME, sif);
}
