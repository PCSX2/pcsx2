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
#include "GSTexture.h"
#include <bitset>

GSTexture::GSTexture()
	: m_scale(1, 1)
	, m_size(0, 0)
	, m_committed_size(0, 0)
	, m_gpu_page_size(0, 0)
	, m_type(Type::Invalid)
	, m_format(Format::Invalid)
	, m_sparse(false)
	, last_frame_used(0)
	, LikelyOffset(false)
	, OffsetHack_modx(0.0f)
	, OffsetHack_mody(0.0f)
{
}

void GSTexture::CommitRegion(const GSVector2i& region)
{
	if (!m_sparse)
		return;

	GSVector2i aligned_region = RoundUpPage(region);
	aligned_region.x = std::max(m_committed_size.x, aligned_region.x);
	aligned_region.y = std::max(m_committed_size.y, aligned_region.y);
	if (aligned_region != m_committed_size)
		CommitPages(aligned_region, true);
}

void GSTexture::Commit()
{
	if (!m_sparse)
		return;

	if (m_committed_size != m_size)
		CommitPages(m_size, true);
}

void GSTexture::Uncommit()
{
	if (!m_sparse)
		return;

	GSVector2i zero = GSVector2i(0, 0);

	if (m_committed_size != zero)
		CommitPages(m_committed_size, false);
}

void GSTexture::SetGpuPageSize(const GSVector2i& page_size)
{
	pxAssert(std::bitset<32>(page_size.x + 1).count() == 1);
	pxAssert(std::bitset<32>(page_size.y + 1).count() == 1);

	m_gpu_page_size = page_size;
}

GSVector2i GSTexture::RoundUpPage(GSVector2i v)
{
	v.x = std::min(m_size.x, v.x);
	v.y = std::min(m_size.y, v.y);
	v.x += m_gpu_page_size.x;
	v.y += m_gpu_page_size.y;
	v.x &= ~m_gpu_page_size.x;
	v.y &= ~m_gpu_page_size.y;

	return v;
}
