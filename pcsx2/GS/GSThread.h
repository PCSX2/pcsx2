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

#pragma once

#include "GS.h"

// http://software.intel.com/en-us/blogs/2012/11/06/exploring-intel-transactional-synchronization-extensions-with-intel-software
#if 0
class TransactionScope
{
public:
	class Lock
	{
		std::atomic<bool> state;

	public:
		Lock()
			: state(false)
		{
		}

		void lock()
		{
			bool expected_value = false;
			while(state.compare_exchange_strong(expected_value, true))
			{
				do {_mm_pause();} while(state);
			}
		}

		void unlock()
		{
			state = false;
		}

		bool isLocked() const
		{
			return state.load();
		}
	};

private:
	Lock& fallBackLock;

	TransactionScope();

public:
	TransactionScope(Lock& fallBackLock_, int max_retries = 3)
		: fallBackLock(fallBackLock_)
	{
		// The TSX (RTM/HLE) instructions on Intel AVX2 CPUs may either be
		// absent or disabled (see errata HSD136 and specification change at
		// http://www.intel.com/content/dam/www/public/us/en/documents/specification-updates/4th-gen-core-family-desktop-specification-update.pdf)
		// This can cause builds for AVX2 CPUs to fail with GCC/Clang on Linux,
		// so check that the RTM instructions are actually available.
#if (_M_SSE >= 0x501 && !defined(__GNUC__)) || defined(__RTM__)

		int nretries = 0;

		while(1)
		{
			++nretries;

			unsigned status = _xbegin();

			if(status == _XBEGIN_STARTED)
			{
				if(!fallBackLock.isLocked()) return;

				_xabort(0xff);
			}

			if((status & _XABORT_EXPLICIT) && _XABORT_CODE(status) == 0xff && !(status & _XABORT_NESTED))
			{
				while(fallBackLock.isLocked()) _mm_pause();
			}
			else if(!(status & _XABORT_RETRY))
			{
				break;
			}

			if(nretries >= max_retries)
			{
				break;
			}
		}

#endif

		fallBackLock.lock();
	}

	~TransactionScope()
	{
		if(fallBackLock.isLocked())
		{
			fallBackLock.unlock();
		}
#if (_M_SSE >= 0x501 && !defined(__GNUC__)) || defined(__RTM__)
		else
		{
			_xend();
		}
#endif
	}
};
#endif
