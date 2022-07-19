#include "Python2QtInputManager.h"

#include <map>

#include "pcsx2/HostSettings.h"
#include "common/SettingsInterface.h"
#include "common/StringUtil.h"


static std::vector<Python2KeyMapping> s_python2_current_mappings;
static std::map<std::string, std::vector<Python2KeyMapping>> mappingsByInputKey;

std::vector<Python2KeyMapping> Python2QtInputManager::GetCurrentMappings()
{
    return s_python2_current_mappings;
}

std::vector<Python2KeyMapping> Python2QtInputManager::GetMappingsByInputKey(std::string keyBindStr)
{
    if (mappingsByInputKey.find(keyBindStr) != mappingsByInputKey.end())
        return mappingsByInputKey[keyBindStr];

    return std::vector<Python2KeyMapping>();
}

bool Python2QtInputManager::AddNewBinding(std::string full_key, std::string new_binding, double analogDeadzone, double analogSensitivity, double motorScale)
{
	uint32_t nextUniqueId = 0;
	for (auto mapping : s_python2_current_mappings)
	{
		if (mapping.uniqueId + 1 > nextUniqueId)
			nextUniqueId = mapping.uniqueId + 1;
	}

	for (auto system_entry : s_python2_system_info)
	{
		if (system_entry.bindings == nullptr)
			continue;

		for (u32 i = 0; i < system_entry.num_bindings; i++)
		{
			auto entry = system_entry.bindings[i];

			if (std::string(entry.name) != full_key)
				continue;

			Python2KeyMapping keybindMapping = {
				nextUniqueId,
				new_binding,
				entry.name,
				entry.type,
				analogDeadzone,
				analogSensitivity,
				motorScale,
				entry.is_oneshot,
			};

			s_python2_current_mappings.push_back(keybindMapping);

			mappingsByInputKey[new_binding].push_back(keybindMapping);

			return true;
		}
	}

    return false;
}

void Python2QtInputManager::RemoveMappingByUniqueId(uint32_t uniqueId)
{
	s_python2_current_mappings.erase(
		std::remove_if(
			s_python2_current_mappings.begin(),
			s_python2_current_mappings.end(),
			[uniqueId](const Python2KeyMapping& x) { return x.uniqueId == uniqueId; }),
		s_python2_current_mappings.end());

	for (auto x : mappingsByInputKey) {
		x.second.erase(
			std::remove_if(
				x.second.begin(),
				x.second.end(),
				[uniqueId](const Python2KeyMapping& x) { return x.uniqueId == uniqueId; }),
			x.second.end());
	}
}

void Python2QtInputManager::LoadMapping()
{
	SettingsInterface* si = Host::GetSettingsInterfaceForBindings();
	const std::string section = "Python2";
	uint32_t uniqueKeybindIdx = 0;

	s_python2_current_mappings.clear();

	for (auto system_entry : s_python2_system_info)
	{
		if (system_entry.bindings == nullptr)
			continue;

		for (u32 i = 0; i < system_entry.num_bindings; i++)
		{
			auto entry = system_entry.bindings[i];
			const std::vector<std::string> bindings(si->GetStringList(section.c_str(), entry.name));

            printf("button: %s\n", entry.name);

			for (auto bind : bindings)
			{
				int isOneshot = 0;
				double analogDeadzone = 0;
				double analogSensitivity = 0;
				double motorScale = 0;

				auto idx = bind.find_first_of(L'|');
				if (idx != std::string::npos)
				{
					auto substr = std::string(bind.begin() + idx + 1, bind.end());

					if (entry.type == PAD::ControllerBindingType::Button)
					{
						isOneshot = std::stoi(substr);
					}
					else if (entry.type == PAD::ControllerBindingType::Axis || entry.type == PAD::ControllerBindingType::HalfAxis)
					{
						analogDeadzone = std::stod(substr);
						analogSensitivity = std::stod(substr.substr(substr.find_first_of('|') + 1));
					}
					else if (entry.type == PAD::ControllerBindingType::Motor)
					{
						motorScale = std::stod(substr);
					}
				}

				auto input_key = std::string(bind.begin(), bind.begin() + idx);

                Python2KeyMapping keybindMapping = {
					uniqueKeybindIdx++,
					input_key,
					std::string(entry.name),
					entry.type,
					analogDeadzone,
					analogSensitivity,
					motorScale,
					isOneshot == 1,
				};

				s_python2_current_mappings.push_back(keybindMapping);

                mappingsByInputKey[input_key].push_back(keybindMapping);

                if (entry.type == PAD::ControllerBindingType::Button)
                    printf("\tbind: %s, oneshot = %d\n", input_key.c_str(), isOneshot);
                else if (entry.type == PAD::ControllerBindingType::Axis || entry.type == PAD::ControllerBindingType::HalfAxis)
                    printf("\tbind: %s, deadzone = %lf, sensitivity = %lf\n", input_key.c_str(), analogDeadzone, analogSensitivity);
                else if (entry.type == PAD::ControllerBindingType::Motor)
                    printf("\tbind: %s, motor scale = %lf\n", input_key.c_str(), motorScale);
			}
		}
	}
}