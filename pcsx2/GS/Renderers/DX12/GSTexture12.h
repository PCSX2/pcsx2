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
#include "GS/Renderers/Common/GSTexture.h"
#include "common/D3D12/Context.h"
#include "common/D3D12/Texture.h"

class GSTexture12 final : public GSTexture
{
public:
	union alignas(16) ClearValue
	{
		float color[4];
		float depth;
	};

public:
	GSTexture12(Type type, Format format, D3D12::Texture texture);
	~GSTexture12() override;

	static std::unique_ptr<GSTexture12> Create(Type type, u32 width, u32 height, u32 levels, Format format,
		DXGI_FORMAT d3d_format, DXGI_FORMAT srv_format, DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format);

	__fi D3D12::Texture& GetTexture() { return m_texture; }
	__fi const D3D12::DescriptorHandle& GetSRVDescriptor() const { return m_texture.GetSRVDescriptor(); }
	__fi const D3D12::DescriptorHandle& GetRTVOrDSVHandle() const { return m_texture.GetRTVOrDSVDescriptor(); }
	__fi D3D12_RESOURCE_STATES GetResourceState() const { return m_texture.GetState(); }
	__fi DXGI_FORMAT GetNativeFormat() const { return m_texture.GetFormat(); }
	__fi ID3D12Resource* GetResource() const { return m_texture.GetResource(); }
	__fi GSVector4 GetClearColor() const { return GSVector4::load<true>(m_clear_value.color); }
	__fi float GetClearDepth() const { return m_clear_value.depth; }

	void* GetNativeHandle() const override;

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) override;
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) override;
	void Unmap() override;
	void GenerateMipmap() override;
	void Swap(GSTexture* tex) override;

	void TransitionToState(D3D12_RESOURCE_STATES state);
	void CommitClear();
	void CommitClear(ID3D12GraphicsCommandList* cmdlist);

	__fi void SetClearColor(const GSVector4& color)
	{
		m_state = State::Cleared;
		GSVector4::store<true>(m_clear_value.color, color);
	}
	__fi void SetClearDepth(float depth)
	{
		m_state = State::Cleared;
		m_clear_value.depth = depth;
	}

	// Call when the texture is bound to the pipeline, or read from in a copy.
	__fi void SetUsedThisCommandBuffer()
	{
		m_use_fence_counter = g_d3d12_context->GetCurrentFenceValue();
	}

private:
	ID3D12GraphicsCommandList* GetCommandBufferForUpdate();
	ID3D12Resource* AllocateUploadStagingBuffer(const void* data, u32 pitch, u32 upload_pitch, u32 height) const;
	void CopyTextureDataForUpload(void* dst, const void* src, u32 pitch, u32 upload_pitch, u32 height) const;

	D3D12::Texture m_texture;

	// Contains the fence counter when the texture was last used.
	// When this matches the current fence counter, the texture was used this command buffer.
	u64 m_use_fence_counter = 0;

	ClearValue m_clear_value = {};

	GSVector4i m_map_area = GSVector4i::zero();
	u32 m_map_level = UINT32_MAX;
};
