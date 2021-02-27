/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#pragma once

#include <mutex>
#include <shared_mutex>
#include <vector>
#include <unordered_map>

#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_OSX == 1
#include <Availability.h>
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101200
#define NO_SHARED_MUTEX
#endif
#endif
#endif

template <class Key, class T>
class ThreadSafeMap
{
#ifdef NO_SHARED_MUTEX
	std::mutex accessMutex;
#else
	std::shared_mutex accessMutex;
#endif

	std::unordered_map<Key, T> map;

public:
	void Add(Key key, T value)
	{
		std::unique_lock modifyLock(accessMutex);
		//Todo, check if key already exists?
		map[key] = value;
	}

	void Remove(Key key)
	{
		std::unique_lock modifyLock(accessMutex);
		map.erase(key);
	}

	void Clear()
	{
		std::unique_lock modifyLock(accessMutex);
		map.clear();
	}

	std::vector<Key> GetKeys()
	{
#ifdef NO_SHARED_MUTEX
		std::unique_lock readLock(accessMutex);
#else
		std::shared_lock readLock(accessMutex);
#endif

		std::vector<Key> keys;
		keys.reserve(map.size());

		for (auto iter = map.begin(); iter != map.end(); ++iter)
			keys.push_back(iter->first);

		return keys;
	}

	//Does not error or insert if no key is found
	bool TryGetValue(Key key, T* value)
	{
#ifdef NO_SHARED_MUTEX
		std::unique_lock readLock(accessMutex);
#else
		std::shared_lock readLock(accessMutex);
#endif
		auto search = map.find(key);
		if (search != map.end())
		{
			*value = map[key];
			return true;
		}
		else
			return false;
	}

	bool ContainsKey(Key key)
	{
#ifdef NO_SHARED_MUTEX
		std::unique_lock readLock(accessMutex);
#else
		std::shared_lock readLock(accessMutex);
#endif
		auto search = map.find(key);
		if (search != map.end())
			return true;
		else
			return false;
	}
};
