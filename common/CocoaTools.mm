// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#if ! __has_feature(objc_arc)
	#error "Compile this with -fobjc-arc"
#endif

#include "CocoaTools.h"
#include "Console.h"
#include "HostSys.h"
#include "WindowInfo.h"
#include <dlfcn.h>
#include <mutex>
#include <vector>
#include <Cocoa/Cocoa.h>
#include <QuartzCore/QuartzCore.h>

// MARK: - Metal Layers

bool CocoaTools::CreateMetalLayer(WindowInfo* wi)
{
	if (![NSThread isMainThread])
	{
		bool ret;
		dispatch_sync(dispatch_get_main_queue(), [&ret, wi]{ ret = CreateMetalLayer(wi); });
		return ret;
	}

	CAMetalLayer* layer = [CAMetalLayer layer];
	if (!layer)
	{
		Console.Error("Failed to create Metal layer.");
		return false;
	}

	NSView* view = (__bridge NSView*)wi->window_handle;
	[view setWantsLayer:YES];
	[view setLayer:layer];
	[layer setContentsScale:[[[view window] screen] backingScaleFactor]];
	// Store the layer pointer, that way MoltenVK doesn't call [NSView layer] outside the main thread.
	wi->surface_handle = (__bridge_retained void*)layer;
	return true;
}

void CocoaTools::DestroyMetalLayer(WindowInfo* wi)
{
	if (![NSThread isMainThread])
	{
		dispatch_sync_f(dispatch_get_main_queue(), wi, [](void* ctx){ DestroyMetalLayer(static_cast<WindowInfo*>(ctx)); });
		return;
	}

	NSView* view = (__bridge NSView*)wi->window_handle;
	CAMetalLayer* layer = (__bridge_transfer CAMetalLayer*)wi->surface_handle;
	if (!layer)
		return;
	wi->surface_handle = nullptr;
	[view setLayer:nil];
	[view setWantsLayer:NO];
}

// MARK: - Theme Change Handlers

@interface PCSX2KVOHelper : NSObject

- (void)addCallback:(void*)ctx run:(void(*)(void*))callback;
- (void)removeCallback:(void*)ctx;

@end

@implementation PCSX2KVOHelper
{
	std::vector<std::pair<void*, void(*)(void*)>> _callbacks;
}

- (void)addCallback:(void*)ctx run:(void(*)(void*))callback
{
	_callbacks.push_back(std::make_pair(ctx, callback));
}

- (void)removeCallback:(void*)ctx
{
	auto new_end = std::remove_if(_callbacks.begin(), _callbacks.end(), [ctx](const auto& entry){
		return ctx == entry.first;
	});
	_callbacks.erase(new_end, _callbacks.end());
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey,id> *)change context:(void *)context
{
	for (const auto& callback : _callbacks)
		callback.second(callback.first);
}

@end

static PCSX2KVOHelper* s_themeChangeHandler;

void CocoaTools::AddThemeChangeHandler(void* ctx, void(handler)(void* ctx))
{
	assert([NSThread isMainThread]);
	if (!s_themeChangeHandler)
	{
		s_themeChangeHandler = [[PCSX2KVOHelper alloc] init];
		NSApplication* app = [NSApplication sharedApplication];
		[app addObserver:s_themeChangeHandler
		      forKeyPath:@"effectiveAppearance"
		         options:0
		         context:nil];
	}
	[s_themeChangeHandler addCallback:ctx run:handler];
}

void CocoaTools::RemoveThemeChangeHandler(void* ctx)
{
	assert([NSThread isMainThread]);
	[s_themeChangeHandler removeCallback:ctx];
}

// MARK: - Sound playback

bool Common::PlaySoundAsync(const char* path)
{
	NSString* nspath = [[NSString alloc] initWithUTF8String:path];
	NSSound* sound = [[NSSound alloc] initWithContentsOfFile:nspath byReference:YES];
	return [sound play];
}

// MARK: - Updater

std::optional<std::string> CocoaTools::GetNonTranslocatedBundlePath()
{
	// See https://objective-see.com/blog/blog_0x15.html

	NSURL* url = [NSURL fileURLWithPath:[[NSBundle mainBundle] bundlePath]];
	if (!url)
		return std::nullopt;

	if (void* handle = dlopen("/System/Library/Frameworks/Security.framework/Security", RTLD_LAZY))
	{
		auto IsTranslocatedURL = reinterpret_cast<Boolean(*)(CFURLRef path, bool* isTranslocated, CFErrorRef*__nullable error)>(dlsym(handle, "SecTranslocateIsTranslocatedURL"));
		auto CreateOriginalPathForURL = reinterpret_cast<CFURLRef __nullable(*)(CFURLRef translocatedPath, CFErrorRef*__nullable error)>(dlsym(handle, "SecTranslocateCreateOriginalPathForURL"));
		bool is_translocated = false;
		if (IsTranslocatedURL)
			IsTranslocatedURL((__bridge CFURLRef)url, &is_translocated, nullptr);
		if (is_translocated)
		{
			if (CFURLRef actual = CreateOriginalPathForURL((__bridge CFURLRef)url, nullptr))
				url = (__bridge_transfer NSURL*)actual;
		}
		dlclose(handle);
	}

	return std::string([url fileSystemRepresentation]);
}

std::optional<std::string> CocoaTools::MoveToTrash(std::string_view file)
{
	NSURL* url = [NSURL fileURLWithPath:[[NSString alloc] initWithBytes:file.data() length:file.size() encoding:NSUTF8StringEncoding]];
	NSURL* new_url;
	if (![[NSFileManager defaultManager] trashItemAtURL:url resultingItemURL:&new_url error:nil])
		return std::nullopt;
	return std::string([new_url fileSystemRepresentation]);
}

bool CocoaTools::DelayedLaunch(std::string_view file)
{
	@autoreleasepool {
		NSTask* task = [NSTask new];
		[task setExecutableURL:[NSURL fileURLWithPath:@"/bin/sh"]];
		[task setEnvironment:@{
			@"WAITPID": [NSString stringWithFormat:@"%d", [[NSProcessInfo processInfo] processIdentifier]],
			@"LAUNCHAPP": [[NSString alloc] initWithBytes:file.data() length:file.size() encoding:NSUTF8StringEncoding],
		}];
		[task setArguments:@[@"-c", @"while /bin/ps -p $WAITPID > /dev/null; do /bin/sleep 0.1; done; exec /usr/bin/open \"$LAUNCHAPP\";"]];
		return [task launchAndReturnError:nil];
	}
}
