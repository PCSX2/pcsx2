/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022 PCSX2 Dev Team
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

#include "GSMTLDeviceInfo.h"
#include "GS/GS.h"
#include "common/Console.h"

#ifdef __APPLE__

static id<MTLLibrary> loadMainLibrary(id<MTLDevice> dev, NSString* name)
{
	NSString* path = [[NSBundle mainBundle] pathForResource:name ofType:@"metallib"];
	return path ? [dev newLibraryWithFile:path error:nullptr] : nullptr;
}

static MRCOwned<id<MTLLibrary>> loadMainLibrary(id<MTLDevice> dev)
{
	if (@available(macOS 11.0, iOS 14.0, *))
		if (id<MTLLibrary> lib = loadMainLibrary(dev, @"Metal23"))
			return MRCTransfer(lib);
	if (@available(macOS 10.15, iOS 13.0, *))
		if (id<MTLLibrary> lib = loadMainLibrary(dev, @"Metal22"))
			return MRCTransfer(lib);
	if (@available(macOS 10.14, iOS 12.0, *))
		if (id<MTLLibrary> lib = loadMainLibrary(dev, @"Metal21"))
			return MRCTransfer(lib);
	return MRCTransfer([dev newDefaultLibrary]);
}

static GSMTLDevice::MetalVersion detectLibraryVersion(id<MTLLibrary> lib)
{
	// These functions are defined in tfx.metal to indicate the metal version used to make the metallib
	if (MRCTransfer([lib newFunctionWithName:@"metal_version_23"]))
		return GSMTLDevice::MetalVersion::Metal23;
	if (MRCTransfer([lib newFunctionWithName:@"metal_version_22"]))
		return GSMTLDevice::MetalVersion::Metal22;
	if (MRCTransfer([lib newFunctionWithName:@"metal_version_21"]))
		return GSMTLDevice::MetalVersion::Metal21;
	return GSMTLDevice::MetalVersion::Metal20;
}

static bool detectPrimIDSupport(id<MTLDevice> dev, id<MTLLibrary> lib)
{
	// Nvidia Metal driver is missing primid support, yay
	MRCOwned<MTLRenderPipelineDescriptor*> desc = MRCTransfer([MTLRenderPipelineDescriptor new]);
	[desc setVertexFunction:MRCTransfer([lib newFunctionWithName:@"fs_triangle"])];
	[desc setFragmentFunction:MRCTransfer([lib newFunctionWithName:@"primid_test"])];
	[[desc colorAttachments][0] setPixelFormat:MTLPixelFormatR8Uint];
	NSError* err;
	[[dev newRenderPipelineStateWithDescriptor:desc error:&err] release];
	return !err;
}

namespace
{
	enum class DetectionResult
	{
		HaswellOrNotIntel, ///< Everything works fine
		Broadwell,         ///< PrimID broken
		Skylake,           ///< PrimID broken, FBFetch supported
	};
}

static DetectionResult detectIntelGPU(id<MTLDevice> dev, id<MTLLibrary> lib)
{
	// Even though it's nowhere in the feature set tables, some Intel GPUs support fbfetch!
	// Annoyingly, the Haswell compiler successfully makes a pipeline but actually miscompiles it and doesn't insert any fbfetch instructions
	// The Broadwell compiler inserts the Skylake fbfetch instruction, but Broadwell doesn't support that.  It seems to make the shader not do anything
	// So we actually have to test the thing
	// In addition, Broadwell+ has broken primid so we need to disable that.
	// Conveniently we can use the same test to detect both (except on macOS < 11.  All Broadwell machines support 11, so the answer to that is "upgrade")
	// See https://github.com/tellowkrinkle/MetalBugReproduction/releases/tag/BrokenPrimID for details

	// AMD compiler crashes and gets retried 3 times over multiple seconds trying to compile the pipeline
	// We know this is only a possibility on Intel anyways
	if (![[dev name] containsString:@"Intel"])
		return DetectionResult::HaswellOrNotIntel;
	auto pdesc = MRCTransfer([MTLRenderPipelineDescriptor new]);
	[pdesc setVertexFunction:MRCTransfer([lib newFunctionWithName:@"fs_triangle"])];
	[pdesc setFragmentFunction:MRCTransfer([lib newFunctionWithName:@"fbfetch_test"])];
	[[pdesc colorAttachments][0] setPixelFormat:MTLPixelFormatRGBA8Unorm];
	auto pipe = MRCTransfer([dev newRenderPipelineStateWithDescriptor:pdesc error:nil]);
	if (!pipe)
		return DetectionResult::HaswellOrNotIntel;
	auto buf = MRCTransfer([dev newBufferWithLength:4 options:MTLResourceStorageModeShared]);
	auto tdesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:false];
	[tdesc setUsage:MTLTextureUsageRenderTarget];
	auto tex = MRCTransfer([dev newTextureWithDescriptor:tdesc]);
	auto q = MRCTransfer([dev newCommandQueue]);
	u32 px = 0x11223344;
	memcpy([buf contents], &px, 4);
	id<MTLCommandBuffer> cmdbuf = [q commandBuffer];
	id<MTLBlitCommandEncoder> upload = [cmdbuf blitCommandEncoder];
	[upload copyFromBuffer:buf sourceOffset:0 sourceBytesPerRow:4 sourceBytesPerImage:4 sourceSize:MTLSizeMake(1, 1, 1) toTexture:tex destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
	[upload endEncoding];
	auto rpdesc = MRCTransfer([MTLRenderPassDescriptor new]);
	[[rpdesc colorAttachments][0] setTexture:tex];
	[[rpdesc colorAttachments][0] setLoadAction:MTLLoadActionLoad];
	[[rpdesc colorAttachments][0] setStoreAction:MTLStoreActionStore];
	id<MTLRenderCommandEncoder> renc = [cmdbuf renderCommandEncoderWithDescriptor:rpdesc];
	[renc setRenderPipelineState:pipe];
	[renc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
	[renc endEncoding];
	id<MTLBlitCommandEncoder> download = [cmdbuf blitCommandEncoder];
	[download copyFromTexture:tex sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(1, 1, 1) toBuffer:buf destinationOffset:0 destinationBytesPerRow:4 destinationBytesPerImage:4];
	[download endEncoding];
	[cmdbuf commit];
	[cmdbuf waitUntilCompleted];
	u32 outpx;
	memcpy(&outpx, [buf contents], 4);
	// Proper fbfetch will double contents, Haswell will return black, and Broadwell will do nothing
	if (outpx == 0x22446688)
		return DetectionResult::Skylake;
	else if (outpx == 0x11223344)
		return DetectionResult::Broadwell;
	else
		return DetectionResult::HaswellOrNotIntel;
}

GSMTLDevice::GSMTLDevice(MRCOwned<id<MTLDevice>> dev)
{
	if (!dev)
		return;
	shaders = loadMainLibrary(dev);

	memset(&features, 0, sizeof(features));

	if (char* env = getenv("MTL_UNIFIED_MEMORY"))
		features.unified_memory = env[0] == '1' || env[0] == 'y' || env[0] == 'Y';
	else if (@available(macOS 10.15, iOS 13.0, *))
		features.unified_memory = [dev hasUnifiedMemory];
	else
		features.unified_memory = false;

	if (@available(macOS 10.15, iOS 13.0, *))
		if ([dev supportsFamily:MTLGPUFamilyMac2] || [dev supportsFamily:MTLGPUFamilyApple1])
			features.texture_swizzle = true;

	if (@available(macOS 11.0, iOS 13.0, *))
		if ([dev supportsFamily:MTLGPUFamilyApple1])
			features.framebuffer_fetch = true;

	features.shader_version = detectLibraryVersion(shaders);
	if (features.framebuffer_fetch && features.shader_version < MetalVersion::Metal23)
	{
		Console.Warning("Metal: GPU supports framebuffer fetch but shader lib does not!  Get an updated shader lib for better performance!");
		features.framebuffer_fetch = false;
	}

	features.primid = features.shader_version >= MetalVersion::Metal22;
	if (features.primid && !detectPrimIDSupport(dev, shaders))
		features.primid = false;

	if (!features.framebuffer_fetch && features.shader_version >= MetalVersion::Metal23)
	{
		switch (detectIntelGPU(dev, shaders))
		{
			case DetectionResult::HaswellOrNotIntel:
				break;
			case DetectionResult::Broadwell:
				features.primid = false; // Broken
				break;
			case DetectionResult::Skylake:
				features.primid = false; // Broken
				features.framebuffer_fetch = true;
				break;
		}
	}

	if (features.framebuffer_fetch && GSConfig.DisableFramebufferFetch)
	{
		Console.Warning("Framebuffer fetch was found but is disabled. This will reduce performance.");
		features.framebuffer_fetch = false;
	}

	if (char* env = getenv("MTL_SLOW_COLOR_COMPRESSION"))
		features.slow_color_compression = env[0] == '1' || env[0] == 'y' || env[0] == 'Y';
	else
		features.slow_color_compression = [[dev name] containsString:@"AMD"];

	features.max_texsize = 8192;
	if ([dev supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v1])
		features.max_texsize = 16384;
	if (@available(macOS 10.15, iOS 13.0, *))
		if ([dev supportsFamily:MTLGPUFamilyApple3])
			features.max_texsize = 16384;

	this->dev = std::move(dev);
}

const char* to_string(GSMTLDevice::MetalVersion ver)
{
	switch (ver)
	{
		case GSMTLDevice::MetalVersion::Metal20: return "Metal 2.0";
		case GSMTLDevice::MetalVersion::Metal21: return "Metal 2.1";
		case GSMTLDevice::MetalVersion::Metal22: return "Metal 2.2";
		case GSMTLDevice::MetalVersion::Metal23: return "Metal 2.3";
	}
}

#endif // __APPLE__
