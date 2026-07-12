// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef TFX_PS_RESOURCES_H
#define TFX_PS_RESOURCES_H

Texture2D<float4> Texture : register(t0);
Texture2D<float4> Palette : register(t1);
Texture2D<float> PrimMinTexture : register(t3);
SamplerState TextureSampler : register(s0);

#ifdef DX12
cbuffer cb1 : register(b1)
#else
cbuffer cb1
#endif
{
	float3 FogColor;
	float AREF;
	float4 WH;
	float2 TA;
	float MaxDepthPS;
	float Af;
	uint4 FbMask;
	float4 HalfTexel;
	float4 MinMax;
	float4 LODParams;
	float4 STRange;
	int4 ChannelShuffle;
	float2 ChannelShuffleOffset;
	float2 TC_OffsetHack;
	float2 STScale;
	float4x4 DitherMatrix;
	float ScaledScaleFactor;
	float RcpScaleFactor;
	float _pad0_cb1;
	float _pad1_cb1;
	float LineCovScale;
	float _pad2_cb1;
	float _pad3_cb1;
	float _pad4_cb1;
};

#endif
