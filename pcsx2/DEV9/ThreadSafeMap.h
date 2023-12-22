// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
