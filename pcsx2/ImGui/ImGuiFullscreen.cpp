// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#define IMGUI_DEFINE_MATH_OPERATORS

#include "fmt/core.h"
#include "Host.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/Common/GSTexture.h"
#include "ImGui/ImGuiAnimated.h"
#include "ImGui/ImGuiFullscreen.h"
#include "ImGui/ImGuiManager.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Easing.h"
#include "common/Image.h"
#include "common/LRUCache.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/Timer.h"

#include "IconsFontAwesome5.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include <array>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <variant>

namespace ImGuiFullscreen
{
	using MessageDialogCallbackVariant = std::variant<InfoMessageDialogCallback, ConfirmMessageDialogCallback>;

	static constexpr float MENU_BACKGROUND_ANIMATION_TIME = 0.5f;

	static std::optional<RGBA8Image> LoadTextureImage(const char* path);
	static std::shared_ptr<GSTexture> UploadTexture(const char* path, const RGBA8Image& image);
	static void TextureLoaderThread();

	static void DrawFileSelector();
	static void DrawChoiceDialog();
	static void DrawInputDialog();
	static void DrawMessageDialog();
	static void DrawBackgroundProgressDialogs(ImVec2& position, float spacing);
	static void DrawNotifications(ImVec2& position, float spacing);
	static void DrawToast();
	static bool MenuButtonFrame(const char* str_id, bool enabled, float height, bool* visible, bool* hovered, ImRect* bb,
		ImGuiButtonFlags flags = 0, float hover_alpha = 1.0f);
	static void PopulateFileSelectorItems();
	static void SetFileSelectorDirectory(std::string dir);
	static ImGuiID GetBackgroundProgressID(const char* str_id);

	ImFont* g_standard_font = nullptr;
	ImFont* g_medium_font = nullptr;
	ImFont* g_large_font = nullptr;
	ImFont* g_icon_font = nullptr;

	float g_layout_scale = 1.0f;
	float g_rcp_layout_scale = 1.0f;
	float g_layout_padding_left = 0.0f;
	float g_layout_padding_top = 0.0f;

	ImVec4 UIBackgroundColor;
	ImVec4 UIBackgroundTextColor;
	ImVec4 UIBackgroundLineColor;
	ImVec4 UIBackgroundHighlightColor;
	ImVec4 UIPopupBackgroundColor;
	ImVec4 UIDisabledColor;
	ImVec4 UIPrimaryColor;
	ImVec4 UIPrimaryLightColor;
	ImVec4 UIPrimaryDarkColor;
	ImVec4 UIPrimaryTextColor;
	ImVec4 UITextHighlightColor;
	ImVec4 UIPrimaryLineColor;
	ImVec4 UISecondaryColor;
	ImVec4 UISecondaryStrongColor;
	ImVec4 UISecondaryWeakColor;
	ImVec4 UISecondaryTextColor;

	static u32 s_menu_button_index = 0;
	static u32 s_close_button_state = 0;
	static bool s_focus_reset_queued = false;
	static bool s_light_theme = false;

	static LRUCache<std::string, std::shared_ptr<GSTexture>> s_texture_cache(128, true);
	static std::shared_ptr<GSTexture> s_placeholder_texture;
	static std::atomic_bool s_texture_load_thread_quit{false};
	static std::mutex s_texture_load_mutex;
	static std::condition_variable s_texture_load_cv;
	static std::deque<std::string> s_texture_load_queue;
	static std::deque<std::pair<std::string, RGBA8Image>> s_texture_upload_queue;
	static Threading::Thread s_texture_load_thread;

	static SmallString s_fullscreen_footer_text;
	static SmallString s_last_fullscreen_footer_text;
	static float s_fullscreen_text_change_time;

	static bool s_choice_dialog_open = false;
	static bool s_choice_dialog_checkable = false;
	static std::string s_choice_dialog_title;
	static ChoiceDialogOptions s_choice_dialog_options;
	static ChoiceDialogCallback s_choice_dialog_callback;
	static ImGuiID s_enum_choice_button_id = 0;
	static s32 s_enum_choice_button_value = 0;
	static bool s_enum_choice_button_set = false;

	static bool s_input_dialog_open = false;
	static std::string s_input_dialog_title;
	static std::string s_input_dialog_message;
	static std::string s_input_dialog_caption;
	static std::string s_input_dialog_text;
	static std::string s_input_dialog_ok_text;
	static InputStringDialogCallback s_input_dialog_callback;

	static bool s_message_dialog_open = false;
	static std::string s_message_dialog_title;
	static std::string s_message_dialog_message;
	static std::array<std::string, 3> s_message_dialog_buttons;
	static MessageDialogCallbackVariant s_message_dialog_callback;

	static ImAnimatedVec2 s_menu_button_frame_min_animated;
	static ImAnimatedVec2 s_menu_button_frame_max_animated;
	static bool s_had_hovered_menu_item = false;
	static bool s_has_hovered_menu_item = false;
	static bool s_rendered_menu_item_border = false;

	struct FileSelectorItem
	{
		FileSelectorItem() = default;
		FileSelectorItem(std::string display_name_, std::string full_path_, bool is_file_)
			: display_name(std::move(display_name_))
			, full_path(std::move(full_path_))
			, is_file(is_file_)
		{
		}
		FileSelectorItem(const FileSelectorItem&) = default;
		FileSelectorItem(FileSelectorItem&&) = default;
		~FileSelectorItem() = default;

		FileSelectorItem& operator=(const FileSelectorItem&) = default;
		FileSelectorItem& operator=(FileSelectorItem&&) = default;

		std::string display_name;
		std::string full_path;
		bool is_file;
	};

	static bool s_file_selector_open = false;
	static bool s_file_selector_directory = false;
	static std::string s_file_selector_title;
	static ImGuiFullscreen::FileSelectorCallback s_file_selector_callback;
	static std::string s_file_selector_current_directory;
	static std::vector<std::string> s_file_selector_filters;
	static std::vector<FileSelectorItem> s_file_selector_items;

	static constexpr float NOTIFICATION_FADE_IN_TIME = 0.2f;
	static constexpr float NOTIFICATION_FADE_OUT_TIME = 0.8f;

	struct Notification
	{
		std::string key;
		std::string title;
		std::string text;
		std::string badge_path;
		Common::Timer::Value start_time;
		Common::Timer::Value move_time;
		float duration;
		float target_y;
		float last_y;
	};

	static std::vector<Notification> s_notifications;

	static std::string s_toast_title;
	static std::string s_toast_message;
	static Common::Timer::Value s_toast_start_time;
	static float s_toast_duration;

	struct BackgroundProgressDialogData
	{
		std::string message;
		ImGuiID id;
		s32 min;
		s32 max;
		s32 value;
	};

	static std::vector<BackgroundProgressDialogData> s_background_progress_dialogs;
	static std::mutex s_background_progress_lock;
} // namespace ImGuiFullscreen

void ImGuiFullscreen::SetFonts(ImFont* standard_font, ImFont* medium_font, ImFont* large_font)
{
	g_standard_font = standard_font;
	g_medium_font = medium_font;
	g_large_font = large_font;
}

bool ImGuiFullscreen::Initialize(const char* placeholder_image_path)
{
	s_focus_reset_queued = true;
	s_close_button_state = 0;

	s_placeholder_texture = LoadTexture(placeholder_image_path);
	if (!s_placeholder_texture)
	{
		Console.Error("Missing placeholder texture '%s', cannot continue", placeholder_image_path);
		return false;
	}

	s_texture_load_thread_quit.store(false, std::memory_order_release);
	s_texture_load_thread.Start(TextureLoaderThread);
	ResetMenuButtonFrame();
	return true;
}

void ImGuiFullscreen::Shutdown(bool clear_state)
{
	if (s_texture_load_thread.Joinable())
	{
		{
			std::unique_lock lock(s_texture_load_mutex);
			s_texture_load_thread_quit.store(true, std::memory_order_release);
			s_texture_load_cv.notify_one();
		}
		s_texture_load_thread.Join();
	}

	s_texture_upload_queue.clear();
	s_placeholder_texture.reset();
	g_standard_font = nullptr;
	g_medium_font = nullptr;
	g_large_font = nullptr;

	s_texture_cache.Clear();

	if (clear_state)
	{
		s_notifications.clear();
		s_background_progress_dialogs.clear();
		s_fullscreen_footer_text.clear();
		s_last_fullscreen_footer_text.clear();
		s_fullscreen_text_change_time = 0.0f;
		CloseInputDialog();
		CloseMessageDialog();
		s_choice_dialog_open = false;
		s_choice_dialog_checkable = false;
		s_choice_dialog_title = {};
		s_choice_dialog_options.clear();
		s_choice_dialog_callback = {};
		s_enum_choice_button_id = 0;
		s_enum_choice_button_value = 0;
		s_enum_choice_button_set = false;
		s_file_selector_open = false;
		s_file_selector_directory = false;
		s_file_selector_title = {};
		s_file_selector_callback = {};
		s_file_selector_current_directory = {};
		s_file_selector_filters.clear();
		s_file_selector_items.clear();
		s_message_dialog_open = false;
		s_message_dialog_title = {};
		s_message_dialog_message = {};
		s_message_dialog_buttons = {};
		s_message_dialog_callback = {};
	}
}

const std::shared_ptr<GSTexture>& ImGuiFullscreen::GetPlaceholderTexture()
{
	return s_placeholder_texture;
}

std::optional<RGBA8Image> ImGuiFullscreen::LoadTextureImage(const char* path)
{
	std::optional<RGBA8Image> image;

	std::optional<std::vector<u8>> data;
	if (Path::IsAbsolute(path))
		data = FileSystem::ReadBinaryFile(path);
	else
		data = FileSystem::ReadBinaryFile(Path::Combine(EmuFolders::Resources, path).c_str());
	if (data.has_value())
	{
		image = RGBA8Image();
		if (!image->LoadFromBuffer(path, data->data(), data->size()))
		{
			Console.Error("Failed to read texture resource '%s'", path);
			image.reset();
		}
	}
	else
	{
		Console.Error("Failed to open texture resource '%s'", path);
	}

	return image;
}

std::shared_ptr<GSTexture> ImGuiFullscreen::UploadTexture(const char* path, const RGBA8Image& image)
{
	GSTexture* texture = g_gs_device->CreateTexture(image.GetWidth(), image.GetHeight(), 1, GSTexture::Format::Color);
	if (!texture)
	{
		Console.Error("failed to create %ux%u texture for resource", image.GetWidth(), image.GetHeight());
		return {};
	}

	if (!texture->Update(GSVector4i(0, 0, image.GetWidth(), image.GetHeight()), image.GetPixels(), image.GetPitch()))
	{
		Console.Error("Failed to upload %ux%u texture for resource", image.GetWidth(), image.GetHeight());
		g_gs_device->Recycle(texture);
		return {};
	}

	DevCon.WriteLn("Uploaded texture resource '%s' (%ux%u)", path, image.GetWidth(), image.GetHeight());
	return std::shared_ptr<GSTexture>(texture, [](GSTexture* tex) { g_gs_device->Recycle(tex); });
}

std::shared_ptr<GSTexture> ImGuiFullscreen::LoadTexture(std::string_view path)
{
	std::string path_str(path);
	std::optional<RGBA8Image> image(LoadTextureImage(path_str.c_str()));
	if (image.has_value())
	{
		std::shared_ptr<GSTexture> ret(UploadTexture(path_str.c_str(), image.value()));
		if (ret)
			return ret;
	}

	return s_placeholder_texture;
}

GSTexture* ImGuiFullscreen::GetCachedTexture(std::string_view name)
{
	std::shared_ptr<GSTexture>* tex_ptr = s_texture_cache.Lookup(name);
	if (!tex_ptr)
	{
		std::shared_ptr<GSTexture> tex(LoadTexture(name));
		tex_ptr = s_texture_cache.Insert(std::string(name), std::move(tex));
	}

	return tex_ptr->get();
}

GSTexture* ImGuiFullscreen::GetCachedTextureAsync(std::string_view name)
{
	std::shared_ptr<GSTexture>* tex_ptr = s_texture_cache.Lookup(name);
	if (!tex_ptr)
	{
		// insert the placeholder
		tex_ptr = s_texture_cache.Insert(std::string(name), s_placeholder_texture);

		// queue the actual load
		std::unique_lock lock(s_texture_load_mutex);
		s_texture_load_queue.emplace_back(name);
		s_texture_load_cv.notify_one();
	}

	return tex_ptr->get();
}

bool ImGuiFullscreen::InvalidateCachedTexture(const std::string& path)
{
	return s_texture_cache.Remove(path);
}

void ImGuiFullscreen::UploadAsyncTextures()
{
	std::unique_lock lock(s_texture_load_mutex);
	while (!s_texture_upload_queue.empty())
	{
		std::pair<std::string, RGBA8Image> it(std::move(s_texture_upload_queue.front()));
		s_texture_upload_queue.pop_front();
		lock.unlock();

		std::shared_ptr<GSTexture> tex = UploadTexture(it.first.c_str(), it.second);
		if (tex)
			s_texture_cache.Insert(std::move(it.first), std::move(tex));

		lock.lock();
	}
}

void ImGuiFullscreen::TextureLoaderThread()
{
	Threading::SetNameOfCurrentThread("ImGuiFullscreen Texture Loader");

	std::unique_lock lock(s_texture_load_mutex);

	for (;;)
	{
		s_texture_load_cv.wait(
			lock, []() { return (s_texture_load_thread_quit.load(std::memory_order_acquire) || !s_texture_load_queue.empty()); });

		if (s_texture_load_thread_quit.load(std::memory_order_acquire))
			break;

		while (!s_texture_load_queue.empty())
		{
			std::string path(std::move(s_texture_load_queue.front()));
			s_texture_load_queue.pop_front();

			lock.unlock();
			std::optional<RGBA8Image> image(LoadTextureImage(path.c_str()));
			lock.lock();

			// don't bother queuing back if it doesn't exist
			if (image)
				s_texture_upload_queue.emplace_back(std::move(path), std::move(image.value()));
		}
	}

	s_texture_load_queue.clear();
}

bool ImGuiFullscreen::UpdateLayoutScale()
{
	static constexpr float LAYOUT_RATIO = LAYOUT_SCREEN_WIDTH / LAYOUT_SCREEN_HEIGHT;
	const ImGuiIO& io = ImGui::GetIO();

	const float screen_width = io.DisplaySize.x;
	const float screen_height = io.DisplaySize.y;
	const float screen_ratio = screen_width / screen_height;
	const float old_scale = g_layout_scale;

	if (screen_ratio > LAYOUT_RATIO)
	{
		// screen is wider, use height, pad width
		g_layout_scale = screen_height / LAYOUT_SCREEN_HEIGHT;
		g_layout_padding_top = 0.0f;
		g_layout_padding_left = (screen_width - (LAYOUT_SCREEN_WIDTH * g_layout_scale)) / 2.0f;
	}
	else
	{
		// screen is taller, use width, pad height
		g_layout_scale = screen_width / LAYOUT_SCREEN_WIDTH;
		g_layout_padding_top = (screen_height - (LAYOUT_SCREEN_HEIGHT * g_layout_scale)) / 2.0f;
		g_layout_padding_left = 0.0f;
	}

	g_rcp_layout_scale = 1.0f / g_layout_scale;

	return g_layout_scale != old_scale;
}

ImRect ImGuiFullscreen::CenterImage(const ImVec2& fit_size, const ImVec2& image_size)
{
	const float fit_ar = fit_size.x / fit_size.y;
	const float image_ar = image_size.x / image_size.y;

	ImRect ret;
	if (fit_ar > image_ar)
	{
		// center horizontally
		const float width = fit_size.y * image_ar;
		const float offset = (fit_size.x - width) / 2.0f;
		const float height = fit_size.y;
		ret = ImRect(ImVec2(offset, 0.0f), ImVec2(offset + width, height));
	}
	else
	{
		// center vertically
		const float height = fit_size.x / image_ar;
		const float offset = (fit_size.y - height) / 2.0f;
		const float width = fit_size.x;
		ret = ImRect(ImVec2(0.0f, offset), ImVec2(width, offset + height));
	}

	return ret;
}

ImRect ImGuiFullscreen::CenterImage(const ImRect& fit_rect, const ImVec2& image_size)
{
	ImRect ret(CenterImage(fit_rect.Max - fit_rect.Min, image_size));
	ret.Translate(fit_rect.Min);
	return ret;
}

void ImGuiFullscreen::BeginLayout()
{
	// we evict from the texture cache at the start of the frame, in case we go over mid-frame,
	// we need to keep all those textures alive until the end of the frame
	s_texture_cache.ManualEvict();

	PushResetLayout();
}

void ImGuiFullscreen::EndLayout()
{
	DrawFileSelector();
	DrawChoiceDialog();
	DrawInputDialog();
	DrawMessageDialog();

	DrawFullscreenFooter();

	const float notification_margin = LayoutScale(10.0f);
	const float spacing = LayoutScale(10.0f);
	const float notification_vertical_pos = GetNotificationVerticalPosition();
	ImVec2 position(notification_margin, notification_vertical_pos * ImGui::GetIO().DisplaySize.y +
											 ((notification_vertical_pos >= 0.5f) ? -notification_margin : notification_margin));
	DrawBackgroundProgressDialogs(position, spacing);
	DrawNotifications(position, spacing);
	DrawToast();

	PopResetLayout();

	s_fullscreen_footer_text.clear();

	s_rendered_menu_item_border = false;
	s_had_hovered_menu_item = std::exchange(s_has_hovered_menu_item, false);
}

void ImGuiFullscreen::PushResetLayout()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(8.0f, 8.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(4.0f, 3.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, LayoutScale(8.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, LayoutScale(4.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, LayoutScale(4.0f, 2.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, LayoutScale(21.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, LayoutScale(14.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, LayoutScale(4.0f));

	ImGui::PushStyleColor(ImGuiCol_Text, UISecondaryTextColor);
	ImGui::PushStyleColor(ImGuiCol_TextDisabled, UIDisabledColor);
	ImGui::PushStyleColor(ImGuiCol_Button, UISecondaryColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIBackgroundColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UIBackgroundHighlightColor);
	ImGui::PushStyleColor(ImGuiCol_Border, UIBackgroundLineColor);
	ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, UIBackgroundColor);
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, UIPrimaryColor);
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, UIPrimaryLightColor);
	ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, UIPrimaryDarkColor);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, UIPopupBackgroundColor);
}

void ImGuiFullscreen::PopResetLayout()
{
	ImGui::PopStyleColor(11);
	ImGui::PopStyleVar(12);
}

void ImGuiFullscreen::QueueResetFocus()
{
	s_focus_reset_queued = true;
	s_close_button_state = 0;
}

bool ImGuiFullscreen::ResetFocusHere()
{
	if (!s_focus_reset_queued)
		return false;

	// don't take focus from dialogs
	if (ImGui::FindBlockingModal(ImGui::GetCurrentWindow()))
		return false;

	s_focus_reset_queued = false;
	ImGui::SetWindowFocus();

	// only do the active selection magic when we're using keyboard/gamepad
	return (GImGui->NavInputSource == ImGuiInputSource_Keyboard || GImGui->NavInputSource == ImGuiInputSource_Gamepad);
}

bool ImGuiFullscreen::IsFocusResetQueued()
{
	return s_focus_reset_queued;
}

void ImGuiFullscreen::ForceKeyNavEnabled()
{
	ImGuiContext& g = *ImGui::GetCurrentContext();
	g.ActiveIdSource = (g.ActiveIdSource == ImGuiInputSource_Mouse) ? ImGuiInputSource_Keyboard : g.ActiveIdSource;
	g.NavInputSource = (g.NavInputSource == ImGuiInputSource_Mouse) ? ImGuiInputSource_Keyboard : g.ActiveIdSource;
	g.NavDisableHighlight = false;
	g.NavDisableMouseHover = true;
}

bool ImGuiFullscreen::WantsToCloseMenu()
{
	// Wait for the Close button to be released, THEN pressed
	if (s_close_button_state == 0)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			s_close_button_state = 1;
		else if (ImGui::IsKeyPressed(ImGuiKey_NavGamepadCancel, false))
			s_close_button_state = 2;
	}
	else if ((s_close_button_state == 1 && ImGui::IsKeyReleased(ImGuiKey_Escape)) ||
			 (s_close_button_state == 2 && ImGui::IsKeyReleased(ImGuiKey_NavGamepadCancel)))
	{
		s_close_button_state = 3;
	}
	return s_close_button_state > 1;
}

void ImGuiFullscreen::ResetCloseMenuIfNeeded()
{
	// If s_close_button_state reached the "Released" state, reset it after the tick
	if (s_close_button_state > 1)
	{
		s_close_button_state = 0;
	}
}

void ImGuiFullscreen::PushPrimaryColor()
{
	ImGui::PushStyleColor(ImGuiCol_Text, UIPrimaryTextColor);
	ImGui::PushStyleColor(ImGuiCol_Button, UIPrimaryDarkColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIPrimaryColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UIPrimaryLightColor);
	ImGui::PushStyleColor(ImGuiCol_Border, UIPrimaryLightColor);
}

void ImGuiFullscreen::PopPrimaryColor()
{
	ImGui::PopStyleColor(5);
}

bool ImGuiFullscreen::BeginFullscreenColumns(const char* title, float pos_y, bool expand_to_screen_width, bool footer)
{
	ImGui::SetNextWindowPos(ImVec2(expand_to_screen_width ? 0.0f : g_layout_padding_left, pos_y));
	ImGui::SetNextWindowSize(ImVec2(
		expand_to_screen_width ? ImGui::GetIO().DisplaySize.x : LayoutScale(LAYOUT_SCREEN_WIDTH),
		ImGui::GetIO().DisplaySize.y - pos_y - (footer ? LayoutScale(LAYOUT_FOOTER_HEIGHT) : 0.0f)));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

	bool clipped;
	if (title)
	{
		ImGui::PushFont(g_large_font);
		clipped = ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
		ImGui::PopFont();
	}
	else
	{
		clipped = ImGui::Begin(
			"fullscreen_ui_columns_parent", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
	}

	return clipped;
}

void ImGuiFullscreen::EndFullscreenColumns()
{
	ImGui::End();
	ImGui::PopStyleVar(3);
}

bool ImGuiFullscreen::BeginFullscreenColumnWindow(float start, float end, const char* name, const ImVec4& background)
{
	start = LayoutScale(start);
	end = LayoutScale(end);

	if (start < 0.0f)
		start = ImGui::GetIO().DisplaySize.x + start;
	if (end <= 0.0f)
		end = ImGui::GetIO().DisplaySize.x + end;

	const ImVec2 pos(start, 0.0f);
	const ImVec2 size(end - start, ImGui::GetCurrentWindow()->Size.y);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, background);

	ImGui::SetCursorPos(pos);

	return ImGui::BeginChild(name, size, false, ImGuiWindowFlags_NavFlattened);
}

void ImGuiFullscreen::EndFullscreenColumnWindow()
{
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

bool ImGuiFullscreen::BeginFullscreenWindow(float left, float top, float width, float height, const char* name,
	const ImVec4& background /* = HEX_TO_IMVEC4(0x212121, 0xFF) */, float rounding /*= 0.0f*/,
	const ImVec2& padding /*= 0.0f*/, ImGuiWindowFlags flags /*= 0*/)
{
	if (left < 0.0f)
		left = (LAYOUT_SCREEN_WIDTH - width) * -left;
	if (top < 0.0f)
		top = (LAYOUT_SCREEN_HEIGHT - height) * -top;

	const ImVec2 pos(ImVec2(LayoutScale(left) + g_layout_padding_left, LayoutScale(top) + g_layout_padding_top));
	const ImVec2 size(LayoutScale(ImVec2(width, height)));
	return BeginFullscreenWindow(pos, size, name, background, rounding, padding, flags);
}

bool ImGuiFullscreen::BeginFullscreenWindow(const ImVec2& position, const ImVec2& size, const char* name,
	const ImVec4& background /* = HEX_TO_IMVEC4(0x212121, 0xFF) */, float rounding /*= 0.0f*/,
	const ImVec2& padding /*= 0.0f*/, ImGuiWindowFlags flags /*= 0*/)
{
	ImGui::SetNextWindowPos(position);
	ImGui::SetNextWindowSize(size);

	ImGui::PushStyleColor(ImGuiCol_WindowBg, background);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(padding));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(rounding));

	return ImGui::Begin(name, nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus |
			flags);
}

void ImGuiFullscreen::EndFullscreenWindow()
{
	ImGui::End();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor();
}

bool ImGuiFullscreen::IsGamepadInputSource()
{
	return (ImGui::GetCurrentContext()->NavInputSource == ImGuiInputSource_Gamepad);
}

void ImGuiFullscreen::CreateFooterTextString(SmallStringBase& dest,
	std::span<const std::pair<const char*, std::string_view>> items)
{
	dest.clear();
	for (const auto& [icon, text] : items)
	{
		if (!dest.empty())
			dest.append("    ");

		dest.append(icon);
		dest.append(' ');
		dest.append(text);
	}
}

void ImGuiFullscreen::SetFullscreenFooterText(std::string_view text)
{
	s_fullscreen_footer_text.assign(text);
}

void ImGuiFullscreen::SetFullscreenFooterText(std::span<const std::pair<const char*, std::string_view>> items)
{
	CreateFooterTextString(s_fullscreen_footer_text, items);
}

void ImGuiFullscreen::DrawFullscreenFooter()
{
	const ImGuiIO& io = ImGui::GetIO();
	if (s_fullscreen_footer_text.empty())
	{
		s_last_fullscreen_footer_text.clear();
		return;
	}

	const float padding = LayoutScale(LAYOUT_FOOTER_PADDING);
	const float height = LayoutScale(LAYOUT_FOOTER_HEIGHT);

	ImDrawList* dl = ImGui::GetForegroundDrawList();
	dl->AddRectFilled(ImVec2(0.0f, io.DisplaySize.y - height), io.DisplaySize, ImGui::GetColorU32(UIPrimaryColor), 0.0f);

	ImFont* const font = g_medium_font;
	const float max_width = io.DisplaySize.x - padding * 2.0f;

	float prev_opacity = 0.0f;
	if (!s_last_fullscreen_footer_text.empty() && s_fullscreen_footer_text != s_last_fullscreen_footer_text)
	{
		if (s_fullscreen_text_change_time == 0.0f)
			s_fullscreen_text_change_time = 0.15f;
		else
			s_fullscreen_text_change_time = std::max(s_fullscreen_text_change_time - io.DeltaTime, 0.0f);

		if (s_fullscreen_text_change_time == 0.0f)
			s_last_fullscreen_footer_text = s_fullscreen_footer_text;

		prev_opacity = s_fullscreen_text_change_time * (1.0f / 0.15f);
		if (prev_opacity > 0.0f)
		{
			const ImVec2 text_size =
				font->CalcTextSizeA(font->FontSize, max_width, 0.0f, s_last_fullscreen_footer_text.c_str(),
					s_last_fullscreen_footer_text.end_ptr());
			dl->AddText(
				font, font->FontSize,
				ImVec2(io.DisplaySize.x - padding * 2.0f - text_size.x, io.DisplaySize.y - font->FontSize - padding),
				ImGui::GetColorU32(ImVec4(UIPrimaryTextColor.x, UIPrimaryTextColor.y, UIPrimaryTextColor.z, prev_opacity)),
				s_last_fullscreen_footer_text.c_str(), s_last_fullscreen_footer_text.end_ptr());
		}
	}
	else if (s_last_fullscreen_footer_text.empty())
	{
		s_last_fullscreen_footer_text = s_fullscreen_footer_text;
	}

	if (prev_opacity < 1.0f)
	{
		const ImVec2 text_size = font->CalcTextSizeA(font->FontSize, max_width, 0.0f, s_fullscreen_footer_text.c_str(),
			s_fullscreen_footer_text.end_ptr());
		dl->AddText(
			font, font->FontSize,
			ImVec2(io.DisplaySize.x - padding * 2.0f - text_size.x, io.DisplaySize.y - font->FontSize - padding),
			ImGui::GetColorU32(ImVec4(UIPrimaryTextColor.x, UIPrimaryTextColor.y, UIPrimaryTextColor.z, 1.0f - prev_opacity)),
			s_fullscreen_footer_text.c_str(), s_fullscreen_footer_text.end_ptr());
	}
}

void ImGuiFullscreen::PrerenderMenuButtonBorder()
{
	if (!s_had_hovered_menu_item)
		return;

	// updating might finish the animation
	const ImVec2& min = s_menu_button_frame_min_animated.UpdateAndGetValue();
	const ImVec2& max = s_menu_button_frame_max_animated.UpdateAndGetValue();
	const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);

	const float t = std::min<float>(std::abs(std::sin(ImGui::GetTime() * 0.75) * 1.1), 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_Border, t));

	ImGui::RenderFrame(min, max, col, true, 0.0f);

	ImGui::PopStyleColor();

	s_rendered_menu_item_border = true;
}

void ImGuiFullscreen::BeginMenuButtons(u32 num_items, float y_align, float x_padding, float y_padding, float item_height)
{
	s_menu_button_index = 0;

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(x_padding, y_padding));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, LayoutScale(1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

	if (y_align != 0.0f)
	{
		const float total_size =
			static_cast<float>(num_items) * LayoutScale(item_height + (y_padding * 2.0f)) + LayoutScale(y_padding * 2.0f);
		const float window_height = ImGui::GetWindowHeight();
		if (window_height > total_size)
			ImGui::SetCursorPosY((window_height - total_size) * y_align);
	}

	PrerenderMenuButtonBorder();
}

void ImGuiFullscreen::EndMenuButtons()
{
	ImGui::PopStyleVar(4);
}

void ImGuiFullscreen::DrawWindowTitle(const char* title)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	const ImVec2 pos(window->DC.CursorPos + LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING));
	const ImVec2 size(window->WorkRect.GetWidth() - (LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING) * 2.0f),
		g_large_font->FontSize + LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING) * 2.0f);
	const ImRect rect(pos, pos + size);

	ImGui::ItemSize(size);
	if (!ImGui::ItemAdd(rect, window->GetID("window_title")))
		return;

	ImGui::PushFont(g_large_font);
	ImGui::RenderTextClipped(rect.Min, rect.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &rect);
	ImGui::PopFont();

	const ImVec2 line_start(pos.x, pos.y + g_large_font->FontSize + LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING));
	const ImVec2 line_end(pos.x + size.x, line_start.y);
	const float line_thickness = LayoutScale(1.0f);
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddLine(line_start, line_end, IM_COL32(255, 255, 255, 255), line_thickness);
}

void ImGuiFullscreen::GetMenuButtonFrameBounds(float height, ImVec2* pos, ImVec2* size)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	*pos = window->DC.CursorPos;
	*size = ImVec2(window->WorkRect.GetWidth(), LayoutScale(height) + ImGui::GetStyle().FramePadding.y * 2.0f);
}

void ImGuiFullscreen::DrawMenuButtonFrame(const ImVec2& p_min, const ImVec2& p_max, ImU32 fill_col,
	bool border /* = true */, float rounding /* = 0.0f */)
{
	ImVec2 frame_min = p_min;
	ImVec2 frame_max = p_max;

	if (ImGui::GetIO().NavVisible && ImGui::GetHoveredID() != ImGui::GetItemID())
	{
		if (!s_had_hovered_menu_item)
		{
			s_menu_button_frame_min_animated.Reset(frame_min);
			s_menu_button_frame_max_animated.Reset(frame_max);
			s_has_hovered_menu_item = true;
		}
		else
		{
			if (frame_min.x != s_menu_button_frame_min_animated.GetEndValue().x ||
				frame_min.y != s_menu_button_frame_min_animated.GetEndValue().y)
			{
				s_menu_button_frame_min_animated.Start(s_menu_button_frame_min_animated.GetCurrentValue(), frame_min,
					MENU_BACKGROUND_ANIMATION_TIME);
			}
			if (frame_max.x != s_menu_button_frame_max_animated.GetEndValue().x ||
				frame_max.y != s_menu_button_frame_max_animated.GetEndValue().y)
			{
				s_menu_button_frame_max_animated.Start(s_menu_button_frame_max_animated.GetCurrentValue(), frame_max,
					MENU_BACKGROUND_ANIMATION_TIME);
			}
			frame_min = s_menu_button_frame_min_animated.UpdateAndGetValue();
			frame_max = s_menu_button_frame_max_animated.UpdateAndGetValue();
			s_has_hovered_menu_item = true;
		}
	}

	if (!s_rendered_menu_item_border)
	{
		s_rendered_menu_item_border = true;
		ImGui::RenderFrame(frame_min, frame_max, fill_col, border, rounding);
	}
}

bool ImGuiFullscreen::MenuButtonFrame(
	const char* str_id, bool enabled, float height, bool* visible, bool* hovered, ImRect* bb, ImGuiButtonFlags flags, float hover_alpha)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
	{
		*visible = false;
		*hovered = false;
		return false;
	}

	ImVec2 pos, size;
	GetMenuButtonFrameBounds(height, &pos, &size);
	*bb = ImRect(pos, pos + size);

	const ImGuiID id = window->GetID(str_id);
	ImGui::ItemSize(size);
	if (enabled)
	{
		if (!ImGui::ItemAdd(*bb, id))
		{
			*visible = false;
			*hovered = false;
			return false;
		}
	}
	else
	{
		if (ImGui::IsClippedEx(*bb, id))
		{
			*visible = false;
			*hovered = false;
			return false;
		}
	}

	*visible = true;

	bool held;
	bool pressed;
	if (enabled)
	{
		pressed = ImGui::ButtonBehavior(*bb, id, hovered, &held, flags);
		if (*hovered)
		{
			const ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered, hover_alpha);

			const float t = std::min<float>(std::abs(std::sin(ImGui::GetTime() * 0.75) * 1.1), 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_Border, t));

			DrawMenuButtonFrame(bb->Min, bb->Max, col, true, 0.0f);

			ImGui::PopStyleColor();
		}
	}
	else
	{
		pressed = false;
		held = false;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	bb->Min += style.FramePadding;
	bb->Max -= style.FramePadding;

	return pressed;
}

bool ImGuiFullscreen::MenuButtonFrame(const char* str_id, bool enabled, float height, bool* visible, bool* hovered, ImVec2* min,
	ImVec2* max, ImGuiButtonFlags flags /*= 0*/, float hover_alpha /*= 0*/)
{
	ImRect bb;
	const bool result = MenuButtonFrame(str_id, enabled, height, visible, hovered, &bb, flags, hover_alpha);
	*min = bb.Min;
	*max = bb.Max;
	return result;
}

void ImGuiFullscreen::ResetMenuButtonFrame()
{
	s_had_hovered_menu_item = false;
	s_has_hovered_menu_item = false;
}

void ImGuiFullscreen::MenuHeading(const char* title, bool draw_line /*= true*/)
{
	const float line_thickness = draw_line ? LayoutScale(1.0f) : 0.0f;
	const float line_padding = draw_line ? LayoutScale(5.0f) : 0.0f;

	bool visible, hovered;
	ImRect bb;
	MenuButtonFrame(title, false, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, &visible, &hovered, &bb);
	if (!visible)
		return;

	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
	ImGui::PushFont(g_large_font);
	ImGui::RenderTextClipped(bb.Min, bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &bb);
	ImGui::PopFont();
	ImGui::PopStyleColor();

	if (draw_line)
	{
		const ImVec2 line_start(bb.Min.x, bb.Min.y + g_large_font->FontSize + line_padding);
		const ImVec2 line_end(bb.Max.x, line_start.y);
		ImGui::GetWindowDrawList()->AddLine(line_start, line_end, ImGui::GetColorU32(ImGuiCol_TextDisabled), line_thickness);
	}
}

bool ImGuiFullscreen::MenuHeadingButton(
	const char* title, const char* value /*= nullptr*/, bool enabled /*= true*/, bool draw_line /*= true*/)
{
	const float line_thickness = draw_line ? LayoutScale(1.0f) : 0.0f;
	const float line_padding = draw_line ? LayoutScale(5.0f) : 0.0f;

	ImRect bb;
	bool visible, hovered;
	bool pressed = MenuButtonFrame(title, enabled, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY, &visible, &hovered, &bb);
	if (!visible)
		return false;

	if (!enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
	ImGui::PushFont(g_large_font);
	ImGui::RenderTextClipped(bb.Min, bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &bb);

	if (value)
	{
		const ImVec2 value_size(g_large_font->CalcTextSizeA(g_large_font->FontSize, std::numeric_limits<float>::max(), 0.0f, value));
		const ImRect value_bb(ImVec2(bb.Max.x - value_size.x, bb.Min.y), ImVec2(bb.Max.x, bb.Max.y));
		ImGui::RenderTextClipped(value_bb.Min, value_bb.Max, value, nullptr, nullptr, ImVec2(0.0f, 0.0f), &value_bb);
	}

	ImGui::PopFont();
	if (!enabled)
		ImGui::PopStyleColor();

	if (draw_line)
	{
		const ImVec2 line_start(bb.Min.x, bb.Min.y + g_large_font->FontSize + line_padding);
		const ImVec2 line_end(bb.Max.x, line_start.y);
		ImGui::GetWindowDrawList()->AddLine(line_start, line_end, ImGui::GetColorU32(ImGuiCol_TextDisabled), line_thickness);
	}

	return pressed;
}

bool ImGuiFullscreen::ActiveButton(const char* title, bool is_active, bool enabled, float height, ImFont* font)
{
	if (is_active)
	{
		ImVec2 pos, size;
		GetMenuButtonFrameBounds(height, &pos, &size);
		ImGui::RenderFrame(pos, pos + size, ImGui::GetColorU32(UIPrimaryColor), false);
	}

	ImRect bb;
	bool visible, hovered;
	bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
	if (!visible)
		return false;

	const ImRect title_bb(bb.GetTL(), bb.GetBR());

	if (!enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

	ImGui::PushFont(font);
	ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
	ImGui::PopFont();

	if (!enabled)
		ImGui::PopStyleColor();

	s_menu_button_index++;
	return pressed;
}

bool ImGuiFullscreen::MenuButton(const char* title, const char* summary, bool enabled, float height, ImFont* font, ImFont* summary_font)
{
	ImRect bb;
	bool visible, hovered;
	bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
	if (!visible)
		return false;

	const float midpoint = bb.Min.y + font->FontSize + LayoutScale(4.0f);
	const ImRect title_bb(bb.Min, ImVec2(bb.Max.x, midpoint));
	const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), bb.Max);

	if (!enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

	ImGui::PushFont(font);
	ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
	ImGui::PopFont();

	if (summary)
	{
		ImGui::PushFont(summary_font);
		ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
		ImGui::PopFont();
	}

	if (!enabled)
		ImGui::PopStyleColor();

	s_menu_button_index++;
	return pressed;
}

bool ImGuiFullscreen::MenuButtonWithoutSummary(const char* title, bool enabled, float height, ImFont* font, const ImVec2& text_align)
{
	ImRect bb;
	bool visible, hovered;
	bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
	if (!visible)
		return false;

	const float midpoint = bb.Min.y + font->FontSize + LayoutScale(4.0f);
	const ImRect title_bb(bb.Min, ImVec2(bb.Max.x, midpoint));
	const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), bb.Max);

	if (!enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

	ImGui::PushFont(font);
	ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, text_align, &title_bb);
	ImGui::PopFont();

	if (!enabled)
		ImGui::PopStyleColor();

	s_menu_button_index++;
	return pressed;
}

bool ImGuiFullscreen::MenuImageButton(const char* title, const char* summary, ImTextureID user_texture_id, const ImVec2& image_size,
	bool enabled, float height, const ImVec2& uv0, const ImVec2& uv1, ImFont* title_font, ImFont* summary_font)
{
	ImRect bb;
	bool visible, hovered;
	bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
	if (!visible)
		return false;

	ImGui::GetWindowDrawList()->AddImage(user_texture_id, bb.Min, bb.Min + image_size, uv0, uv1,
		enabled ? IM_COL32(255, 255, 255, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled));

	const float midpoint = bb.Min.y + title_font->FontSize + LayoutScale(4.0f);
	const float text_start_x = bb.Min.x + image_size.x + LayoutScale(15.0f);
	const ImRect title_bb(ImVec2(text_start_x, bb.Min.y), ImVec2(bb.Max.x, midpoint));
	const ImRect summary_bb(ImVec2(text_start_x, midpoint), bb.Max);

	if (!enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

	ImGui::PushFont(title_font);
	ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
	ImGui::PopFont();

	if (summary)
	{
		ImGui::PushFont(summary_font);
		ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
		ImGui::PopFont();
	}

	if (!enabled)
		ImGui::PopStyleColor();

	s_menu_button_index++;
	return pressed;
}

bool ImGuiFullscreen::FloatingButton(const char* text, float x, float y, float width, float height, float anchor_x, float anchor_y,
	bool enabled, ImFont* font, ImVec2* out_position, bool repeat_button)
{
	const ImVec2 text_size(font->CalcTextSizeA(font->FontSize, std::numeric_limits<float>::max(), 0.0f, text));
	const ImVec2& padding(ImGui::GetStyle().FramePadding);
	if (width < 0.0f)
		width = (padding.x * 2.0f) + text_size.x;
	if (height < 0.0f)
		height = (padding.y * 2.0f) + text_size.y;

	const ImVec2 window_size(ImGui::GetWindowSize());
	if (anchor_x == -1.0f)
		x -= width;
	else if (anchor_x == -0.5f)
		x -= (width * 0.5f);
	else if (anchor_x == 0.5f)
		x = (window_size.x * 0.5f) - (width * 0.5f) - x;
	else if (anchor_x == 1.0f)
		x = window_size.x - width - x;
	if (anchor_y == -1.0f)
		y -= height;
	else if (anchor_y == -0.5f)
		y -= (height * 0.5f);
	else if (anchor_y == 0.5f)
		y = (window_size.y * 0.5f) - (height * 0.5f) - y;
	else if (anchor_y == 1.0f)
		y = window_size.y - height - y;

	if (out_position)
		*out_position = ImVec2(x, y);

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	const ImVec2 base(ImGui::GetWindowPos() + ImVec2(x, y));
	ImRect bb(base, base + ImVec2(width, height));

	const ImGuiID id = window->GetID(text);
	if (enabled)
	{
		if (!ImGui::ItemAdd(bb, id))
			return false;
	}
	else
	{
		if (ImGui::IsClippedEx(bb, id))
			return false;
	}

	bool hovered;
	bool held;
	bool pressed;
	if (enabled)
	{
		pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, repeat_button ? ImGuiButtonFlags_Repeat : 0);
		if (hovered)
		{
			const float t = std::min<float>(std::abs(std::sin(ImGui::GetTime() * 0.75) * 1.1), 1.0f);
			const ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_Border, t));
			DrawMenuButtonFrame(bb.Min, bb.Max, col, true, 0.0f);
			ImGui::PopStyleColor();
		}
	}
	else
	{
		hovered = false;
		pressed = false;
		held = false;
	}

	bb.Min += padding;
	bb.Max -= padding;

	if (!enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

	ImGui::PushFont(font);
	ImGui::RenderTextClipped(bb.Min, bb.Max, text, nullptr, nullptr, ImVec2(0.0f, 0.0f), &bb);
	ImGui::PopFont();

	if (!enabled)
		ImGui::PopStyleColor();

	return pressed;
}

bool ImGuiFullscreen::ToggleButton(
	const char* title, const char* summary, bool* v, bool enabled, float height, ImFont* font, ImFont* summary_font)
{
	ImRect bb;
	bool visible, hovered;
	bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb, ImGuiButtonFlags_PressedOnClick);
	if (!visible)
		return false;

	const float midpoint = bb.Min.y + font->FontSize + LayoutScale(4.0f);
	const ImRect title_bb(bb.Min, ImVec2(bb.Max.x, midpoint));
	const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), bb.Max);

	if (!enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

	ImGui::PushFont(font);
	ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
	ImGui::PopFont();

	if (summary)
	{
		ImGui::PushFont(summary_font);
		ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
		ImGui::PopFont();
	}

	if (!enabled)
		ImGui::PopStyleColor();

	const float toggle_width = LayoutScale(50.0f);
	const float toggle_height = LayoutScale(25.0f);
	const float toggle_x = LayoutScale(8.0f);
	const float toggle_y = (LayoutScale(height) - toggle_height) * 0.5f;
	const float toggle_radius = toggle_height * 0.5f;
	const ImVec2 toggle_pos(bb.Max.x - toggle_width - toggle_x, bb.Min.y + toggle_y);

	if (pressed)
		*v = !*v;

	float t = *v ? 1.0f : 0.0f;
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImGuiContext& g = *GImGui;
	if (g.LastActiveId == g.CurrentWindow->GetID(title))
	{
		static constexpr const float ANIM_SPEED = 0.08f;
		float t_anim = ImSaturate(g.LastActiveIdTimer / ANIM_SPEED);
		t = *v ? (t_anim) : (1.0f - t_anim);
	}

	ImU32 col_bg;
	ImU32 col_knob;
	if (!enabled)
	{
		col_bg = ImGui::GetColorU32(UIDisabledColor);
		col_knob = IM_COL32(200, 200, 200, 200);
	}
	else
	{
		col_bg = ImGui::GetColorU32(ImLerp(HEX_TO_IMVEC4(0x8C8C8C, 0xff), UISecondaryStrongColor, t));
		col_knob = IM_COL32(255, 255, 255, 255);
	}

	dl->AddRectFilled(toggle_pos, ImVec2(toggle_pos.x + toggle_width, toggle_pos.y + toggle_height), col_bg, toggle_height * 0.5f);
	dl->AddCircleFilled(ImVec2(toggle_pos.x + toggle_radius + t * (toggle_width - toggle_radius * 2.0f), toggle_pos.y + toggle_radius),
		toggle_radius - 1.5f, col_knob, 32);

	s_menu_button_index++;
	return pressed;
}

bool ImGuiFullscreen::ThreeWayToggleButton(
	const char* title, const char* summary, std::optional<bool>* v, bool enabled, float height, ImFont* font, ImFont* summary_font)
{
	ImRect bb;
	bool visible, hovered;
	bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb, ImGuiButtonFlags_PressedOnClick);
	if (!visible)
		return false;

	const float midpoint = bb.Min.y + font->FontSize + LayoutScale(4.0f);
	const ImRect title_bb(bb.Min, ImVec2(bb.Max.x, midpoint));
	const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), bb.Max);

	if (!enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

	ImGui::PushFont(font);
	ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
	ImGui::PopFont();

	if (summary)
	{
		ImGui::PushFont(summary_font);
		ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
		ImGui::PopFont();
	}

	if (!enabled)
		ImGui::PopStyleColor();

	const float toggle_width = LayoutScale(50.0f);
	const float toggle_height = LayoutScale(25.0f);
	const float toggle_x = LayoutScale(8.0f);
	const float toggle_y = (LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT) - toggle_height) * 0.5f;
	const float toggle_radius = toggle_height * 0.5f;
	const ImVec2 toggle_pos(bb.Max.x - toggle_width - toggle_x, bb.Min.y + toggle_y);

	if (pressed)
	{
		if (v->has_value() && v->value())
			*v = false;
		else if (v->has_value() && !v->value())
			v->reset();
		else
			*v = true;
	}

	float t = v->has_value() ? (v->value() ? 1.0f : 0.0f) : 0.5f;
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImGuiContext& g = *GImGui;
	if (g.LastActiveId == g.CurrentWindow->GetID(title))
	{
		static constexpr const float ANIM_SPEED = 0.08f;
		float t_anim = ImSaturate(g.LastActiveIdTimer / ANIM_SPEED);
		t = (v->has_value() ? (v->value() ? std::min(t_anim + 0.5f, 1.0f) : (1.0f - t_anim)) : (t_anim * 0.5f));
	}

	const float color_t = v->has_value() ? t : 0.0f;

	ImU32 col_bg;
	if (!enabled)
		col_bg = IM_COL32(0x75, 0x75, 0x75, 0xff);
	else if (hovered)
		col_bg = ImGui::GetColorU32(
			ImLerp(v->has_value() ? HEX_TO_IMVEC4(0xf05100, 0xff) : HEX_TO_IMVEC4(0x9e9e9e, 0xff), UISecondaryStrongColor, color_t));
	else
		col_bg = ImGui::GetColorU32(
			ImLerp(v->has_value() ? HEX_TO_IMVEC4(0xc45100, 0xff) : HEX_TO_IMVEC4(0x757575, 0xff), UISecondaryStrongColor, color_t));

	dl->AddRectFilled(toggle_pos, ImVec2(toggle_pos.x + toggle_width, toggle_pos.y + toggle_height), col_bg, toggle_height * 0.5f);
	dl->AddCircleFilled(ImVec2(toggle_pos.x + toggle_radius + t * (toggle_width - toggle_radius * 2.0f), toggle_pos.y + toggle_radius),
		toggle_radius - 1.5f, IM_COL32(255, 255, 255, 255), 32);

	s_menu_button_index++;
	return pressed;
}

bool ImGuiFullscreen::MenuButtonWithValue(
	const char* title, const char* summary, const char* value, bool enabled, float height, ImFont* font, ImFont* summary_font)
{
	ImRect bb;
	bool visible, hovered;
	bool pressed = MenuButtonFrame(title, enabled, height, &visible, &hovered, &bb);
	if (!visible)
		return false;

	const ImVec2 value_size(ImGui::CalcTextSize(value));

	const float midpoint = bb.Min.y + font->FontSize + LayoutScale(4.0f);
	const float text_end = bb.Max.x - value_size.x;
	const ImRect title_bb(bb.Min, ImVec2(text_end, midpoint));
	const ImRect summary_bb(ImVec2(bb.Min.x, midpoint), ImVec2(text_end, bb.Max.y));

	if (!enabled)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));

	ImGui::PushFont(font);
	ImGui::RenderTextClipped(title_bb.Min, title_bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &title_bb);
	ImGui::RenderTextClipped(bb.Min, bb.Max, value, nullptr, nullptr, ImVec2(1.0f, 0.5f), &bb);
	ImGui::PopFont();

	if (summary)
	{
		ImGui::PushFont(summary_font);
		ImGui::RenderTextClipped(summary_bb.Min, summary_bb.Max, summary, nullptr, nullptr, ImVec2(0.0f, 0.0f), &summary_bb);
		ImGui::PopFont();
	}

	if (!enabled)
		ImGui::PopStyleColor();

	return pressed;
}

bool ImGuiFullscreen::EnumChoiceButtonImpl(const char* title, const char* summary, s32* value_pointer,
	const char* (*to_display_name_function)(s32 value, void* opaque), void* opaque, u32 count, bool enabled, float height, ImFont* font,
	ImFont* summary_font)
{
	const bool pressed =
		MenuButtonWithValue(title, summary, to_display_name_function(*value_pointer, opaque), enabled, height, font, summary_font);

	if (pressed)
	{
		s_enum_choice_button_id = ImGui::GetID(title);
		s_enum_choice_button_value = *value_pointer;
		s_enum_choice_button_set = false;

		ChoiceDialogOptions options;
		options.reserve(count);
		for (u32 i = 0; i < count; i++)
			options.emplace_back(to_display_name_function(static_cast<s32>(i), opaque), static_cast<u32>(*value_pointer) == i);
		OpenChoiceDialog(title, false, std::move(options), [](s32 index, const std::string& title, bool checked) {
			if (index >= 0)
				s_enum_choice_button_value = index;

			s_enum_choice_button_set = true;
			CloseChoiceDialog();
		});
	}

	bool changed = false;
	if (s_enum_choice_button_set && s_enum_choice_button_id == ImGui::GetID(title))
	{
		changed = s_enum_choice_button_value != *value_pointer;
		if (changed)
			*value_pointer = s_enum_choice_button_value;

		s_enum_choice_button_id = 0;
		s_enum_choice_button_value = 0;
		s_enum_choice_button_set = false;
	}

	return changed;
}

void ImGuiFullscreen::BeginNavBar(float x_padding /*= LAYOUT_MENU_BUTTON_X_PADDING*/, float y_padding /*= LAYOUT_MENU_BUTTON_Y_PADDING*/)
{
	s_menu_button_index = 0;

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(x_padding, y_padding));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, LayoutScale(1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, LayoutScale(1.0f, 0.0f));
	PushPrimaryColor();
}

void ImGuiFullscreen::EndNavBar()
{
	PopPrimaryColor();
	ImGui::PopStyleVar(4);
}

void ImGuiFullscreen::NavTitle(const char* title, float height /*= LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY*/, ImFont* font /*= g_large_font*/)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return;

	s_menu_button_index++;

	const ImVec2 text_size(font->CalcTextSizeA(font->FontSize, std::numeric_limits<float>::max(), 0.0f, title));
	const ImVec2 pos(window->DC.CursorPos);
	const ImGuiStyle& style = ImGui::GetStyle();
	const ImVec2 size = ImVec2(text_size.x, LayoutScale(height) + style.FramePadding.y * 2.0f);

	ImGui::ItemSize(ImVec2(size.x + style.FrameBorderSize + style.ItemSpacing.x, size.y + style.FrameBorderSize + style.ItemSpacing.y));
	ImGui::SameLine();

	ImRect bb(pos, pos + size);
	if (ImGui::IsClippedEx(bb, 0))
		return;

	bb.Min.y += style.FramePadding.y;
	bb.Max.y -= style.FramePadding.y;

	ImGui::PushFont(font);
	ImGui::RenderTextClipped(bb.Min, bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &bb);
	ImGui::PopFont();
}

void ImGuiFullscreen::RightAlignNavButtons(u32 num_items /*= 0*/, float item_width /*= LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY*/,
	float item_height /*= LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY*/)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	const ImGuiStyle& style = ImGui::GetStyle();

	const float total_item_width = style.FramePadding.x * 2.0f + style.FrameBorderSize + style.ItemSpacing.x + LayoutScale(item_width);
	const float margin = total_item_width * static_cast<float>(num_items);
	ImGui::SetCursorPosX(window->InnerClipRect.Max.x - margin - style.FramePadding.x);
}

bool ImGuiFullscreen::NavButton(const char* title, bool is_active, bool enabled /* = true */, float width /* = -1.0f */,
	float height /* = LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY */, ImFont* font /* = g_large_font */)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	s_menu_button_index++;

	const ImVec2 text_size(font->CalcTextSizeA(font->FontSize, std::numeric_limits<float>::max(), 0.0f, title));
	const ImVec2 pos(window->DC.CursorPos);
	const ImGuiStyle& style = ImGui::GetStyle();
	const ImVec2 size = ImVec2(((width < 0.0f) ? text_size.x : LayoutScale(width)) + style.FramePadding.x * 2.0f,
		LayoutScale(height) + style.FramePadding.y * 2.0f);

	ImGui::ItemSize(ImVec2(size.x + style.FrameBorderSize + style.ItemSpacing.x, size.y + style.FrameBorderSize + style.ItemSpacing.y));
	ImGui::SameLine();

	ImRect bb(pos, pos + size);
	const ImGuiID id = window->GetID(title);
	if (enabled)
	{
		// bit contradictory - we don't want this button to be used for *gamepad* navigation, since they're usually
		// activated with the bumpers and/or the back button.
		if (!ImGui::ItemAdd(bb, id, nullptr, ImGuiItemFlags_NoNav | ImGuiItemFlags_NoNavDefaultFocus))
			return false;
	}
	else
	{
		if (ImGui::IsClippedEx(bb, id))
			return false;
	}

	bool held;
	bool pressed;
	bool hovered;
	if (enabled)
	{
		pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_NoNavFocus);
		if (hovered)
		{
			const ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered, 1.0f);
			DrawMenuButtonFrame(bb.Min, bb.Max, col, true, 0.0f);
		}
	}
	else
	{
		pressed = false;
		held = false;
		hovered = false;
	}

	bb.Min += style.FramePadding;
	bb.Max -= style.FramePadding;

	ImGui::PushStyleColor(
		ImGuiCol_Text, ImGui::GetColorU32(enabled ? (is_active ? ImGuiCol_Text : ImGuiCol_TextDisabled) : ImGuiCol_ButtonHovered));

	ImGui::PushFont(font);
	ImGui::RenderTextClipped(bb.Min, bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &bb);
	ImGui::PopFont();

	ImGui::PopStyleColor();

	return pressed;
}


bool ImGuiFullscreen::NavTab(const char* title, bool is_active, bool enabled /* = true */, float width, float height,
	const ImVec4& background, ImFont* font /* = g_large_font */)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	s_menu_button_index++;

	const ImVec2 text_size(font->CalcTextSizeA(font->FontSize, std::numeric_limits<float>::max(), 0.0f, title));
	const ImVec2 pos(window->DC.CursorPos);
	const ImVec2 size = ImVec2(((width < 0.0f) ? text_size.x : LayoutScale(width)), LayoutScale(height));

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::ItemSize(ImVec2(size.x, size.y));
	ImGui::SameLine();
	ImGui::PopStyleVar();

	ImRect bb(pos, pos + size);
	const ImGuiID id = window->GetID(title);
	if (enabled)
	{
		// bit contradictory - we don't want this button to be used for *gamepad* navigation, since they're usually
		// activated with the bumpers and/or the back button.
		if (!ImGui::ItemAdd(bb, id, nullptr, ImGuiItemFlags_NoNav | ImGuiItemFlags_NoNavDefaultFocus))
			return false;
	}
	else
	{
		if (ImGui::IsClippedEx(bb, id))
			return false;
	}

	bool held;
	bool pressed;
	bool hovered;
	if (enabled)
	{
		pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_NoNavFocus);
	}
	else
	{
		pressed = false;
		held = false;
		hovered = false;
	}

	const ImU32 col =
		hovered ? ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered, 1.0f) :
				  ImGui::GetColorU32(is_active ? background : ImVec4(background.x, background.y, background.z, 0.5f));

	DrawMenuButtonFrame(bb.Min, bb.Max, col, true, 0.0f);

#if 0
	// This looks a bit rubbish... but left it here if someone thinks they can improve it.
	if (is_active)
	{
		const float line_thickness = LayoutScale(2.0f);
		ImGui::GetWindowDrawList()->AddLine(ImVec2(bb.Min.x, bb.Max.y - line_thickness),
			ImVec2(bb.Max.x, bb.Max.y - line_thickness),
			ImGui::GetColorU32(ImGuiCol_TextDisabled), line_thickness);
	}
#endif

	const ImVec2 pad(std::max((size.x - text_size.x) * 0.5f, 0.0f), std::max((size.y - text_size.y) * 0.5f, 0.0f));
	bb.Min += pad;
	bb.Max -= pad;

	ImGui::PushStyleColor(
		ImGuiCol_Text,
		ImGui::GetColorU32(enabled ? (is_active ? ImGuiCol_Text : ImGuiCol_TextDisabled) : ImGuiCol_ButtonHovered));

	ImGui::PushFont(font);
	ImGui::RenderTextClipped(bb.Min, bb.Max, title, nullptr, nullptr, ImVec2(0.0f, 0.0f), &bb);
	ImGui::PopFont();

	ImGui::PopStyleColor();

	return pressed;
}

bool ImGuiFullscreen::BeginHorizontalMenu(const char* name, const ImVec2& position, const ImVec2& size, u32 num_items)
{
	s_menu_button_index = 0;

	const float item_padding = LayoutScale(LAYOUT_HORIZONTAL_MENU_PADDING);
	const float item_width = LayoutScale(LAYOUT_HORIZONTAL_MENU_ITEM_WIDTH);
	const float item_spacing = LayoutScale(30.0f);
	const float menu_width = static_cast<float>(num_items) * (item_width + item_spacing) - item_spacing;
	const float menu_height = LayoutScale(LAYOUT_HORIZONTAL_MENU_HEIGHT);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(item_padding, item_padding));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, LayoutScale(1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(item_spacing, 0.0f));

	if (!BeginFullscreenWindow(position, size, name, UIBackgroundColor, 0.0f, ImVec2()))
		return false;

	ImGui::SetCursorPos(ImVec2((size.x - menu_width) * 0.5f, (size.y - menu_height) * 0.5f));

	PrerenderMenuButtonBorder();
	return true;
}

void ImGuiFullscreen::EndHorizontalMenu()
{
	ImGui::PopStyleVar(4);
	EndFullscreenWindow();
}

bool ImGuiFullscreen::HorizontalMenuItem(GSTexture* icon, const char* title, const char* description)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	const ImVec2 pos = window->DC.CursorPos;
	const ImVec2 size = LayoutScale(LAYOUT_HORIZONTAL_MENU_ITEM_WIDTH, LAYOUT_HORIZONTAL_MENU_HEIGHT);
	ImRect bb = ImRect(pos, pos + size);

	const ImGuiID id = window->GetID(title);
	ImGui::ItemSize(size);
	if (!ImGui::ItemAdd(bb, id))
		return false;

	bool held;
	bool hovered;
	const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
	if (hovered)
	{
		const ImU32 col = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered, 1.0f);

		const float t = static_cast<float>(std::min(std::abs(std::sin(ImGui::GetTime() * 0.75) * 1.1), 1.0));
		ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetColorU32(ImGuiCol_Border, t));

		DrawMenuButtonFrame(bb.Min, bb.Max, col, true, 0.0f);

		ImGui::PopStyleColor();
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	bb.Min += style.FramePadding;
	bb.Max -= style.FramePadding;

	const float avail_width = bb.Max.x - bb.Min.x;
	const float icon_size = LayoutScale(150.0f);
	const ImVec2 icon_pos = bb.Min + ImVec2((avail_width - icon_size) * 0.5f, 0.0f);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddImage(icon->GetNativeHandle(), icon_pos, icon_pos + ImVec2(icon_size, icon_size));

	ImFont* title_font = g_large_font;
	const ImVec2 title_size = title_font->CalcTextSizeA(title_font->FontSize, avail_width, 0.0f, title);
	const ImVec2 title_pos =
		ImVec2(bb.Min.x + (avail_width - title_size.x) * 0.5f, icon_pos.y + icon_size + LayoutScale(10.0f));
	const ImVec4 title_bb = ImVec4(title_pos.x, title_pos.y, title_pos.x + title_size.x, title_pos.y + title_size.y);

	dl->AddText(title_font, title_font->FontSize, title_pos, ImGui::GetColorU32(ImGuiCol_Text), title, nullptr, 0.0f,
		&title_bb);

	ImFont* desc_font = g_medium_font;
	const ImVec2 desc_size = desc_font->CalcTextSizeA(desc_font->FontSize, avail_width, avail_width, description);
	const ImVec2 desc_pos = ImVec2(bb.Min.x + (avail_width - desc_size.x) * 0.5f, title_bb.w + LayoutScale(10.0f));
	const ImVec4 desc_bb = ImVec4(desc_pos.x, desc_pos.y, desc_pos.x + desc_size.x, desc_pos.y + desc_size.y);

	dl->AddText(desc_font, desc_font->FontSize, desc_pos, ImGui::GetColorU32(ImGuiCol_Text), description, nullptr,
		avail_width, &desc_bb);

	ImGui::SameLine();

	s_menu_button_index++;
	return pressed;
}

void ImGuiFullscreen::PopulateFileSelectorItems()
{
	s_file_selector_items.clear();

	if (s_file_selector_current_directory.empty())
	{
		for (std::string& root_path : FileSystem::GetRootDirectoryList())
		{
			std::string title(fmt::format(ICON_FA_FOLDER " {}", root_path));
			s_file_selector_items.emplace_back(std::move(title), std::move(root_path), false);
		}
	}
	else
	{
		FileSystem::FindResultsArray results;
		FileSystem::FindFiles(s_file_selector_current_directory.c_str(), "*",
			FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES | FILESYSTEM_FIND_RELATIVE_PATHS, &results);

		std::string parent_path;
		std::string::size_type sep_pos = s_file_selector_current_directory.rfind(FS_OSPATH_SEPARATOR_CHARACTER);
		if (sep_pos != std::string::npos)
		{
			parent_path = s_file_selector_current_directory.substr(0, sep_pos);

#ifndef _WIN32
			// Special case for going to root list on Linux.
			if (parent_path.empty() && s_file_selector_current_directory.size() > 1)
				parent_path = "/";
#endif
		}

		s_file_selector_items.emplace_back(ICON_FA_FOLDER_OPEN " <Parent Directory>", std::move(parent_path), false);
		std::sort(results.begin(), results.end(), [](const FILESYSTEM_FIND_DATA& lhs, const FILESYSTEM_FIND_DATA& rhs) {
			if ((lhs.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY) != (rhs.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY))
				return (lhs.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY) != 0;

			return std::lexicographical_compare(lhs.FileName.begin(), lhs.FileName.end(), rhs.FileName.begin(), rhs.FileName.end());
		});

		for (const FILESYSTEM_FIND_DATA& fd : results)
		{
			std::string full_path(Path::Combine(s_file_selector_current_directory, fd.FileName));

			if (fd.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY)
			{
				std::string title(fmt::format(ICON_FA_FOLDER " {}", fd.FileName));
				s_file_selector_items.emplace_back(std::move(title), std::move(full_path), false);
			}
			else
			{
				if (s_file_selector_filters.empty() ||
					std::none_of(s_file_selector_filters.begin(), s_file_selector_filters.end(),
						[&fd](const std::string& filter) { return StringUtil::WildcardMatch(fd.FileName.c_str(), filter.c_str(), false); }))
				{
					continue;
				}

				std::string title(fmt::format(ICON_FA_FILE " {}", fd.FileName));
				s_file_selector_items.emplace_back(std::move(title), std::move(full_path), true);
			}
		}
	}
}

void ImGuiFullscreen::SetFileSelectorDirectory(std::string dir)
{
	while (dir.size() > 1 && dir.back() == FS_OSPATH_SEPARATOR_CHARACTER)
		dir.erase(dir.size() - 1);

	s_file_selector_current_directory = std::move(dir);
	PopulateFileSelectorItems();
}

bool ImGuiFullscreen::IsFileSelectorOpen()
{
	return s_file_selector_open;
}

void ImGuiFullscreen::OpenFileSelector(std::string_view title, bool select_directory, FileSelectorCallback callback,
	FileSelectorFilters filters, std::string initial_directory)
{
	if (initial_directory.empty() || !FileSystem::DirectoryExists(initial_directory.c_str()))
		initial_directory = FileSystem::GetWorkingDirectory();

	if (Host::ShouldPreferHostFileSelector())
	{
		Host::OpenHostFileSelectorAsync(ImGuiManager::StripIconCharacters(title), select_directory, std::move(callback),
			std::move(filters), initial_directory);
		return;
	}

	if (s_file_selector_open)
		CloseFileSelector();

	s_file_selector_open = true;
	s_file_selector_directory = select_directory;
	s_file_selector_title = fmt::format("{}##file_selector", title);
	s_file_selector_callback = std::move(callback);
	s_file_selector_filters = std::move(filters);

	SetFileSelectorDirectory(std::move(initial_directory));
	QueueResetFocus();
}

void ImGuiFullscreen::CloseFileSelector()
{
	if (!s_file_selector_open)
		return;

	s_file_selector_open = false;
	s_file_selector_directory = false;
	std::string().swap(s_file_selector_title);
	FileSelectorCallback().swap(s_file_selector_callback);
	FileSelectorFilters().swap(s_file_selector_filters);
	std::string().swap(s_file_selector_current_directory);
	s_file_selector_items.clear();
	ImGui::CloseCurrentPopup();
	QueueResetFocus();
}

void ImGuiFullscreen::DrawFileSelector()
{
	if (!s_file_selector_open)
		return;

	ImGui::SetNextWindowSize(LayoutScale(1000.0f, 680.0f));
	ImGui::SetNextWindowPos((ImGui::GetIO().DisplaySize - LayoutScale(0.0f, LAYOUT_FOOTER_HEIGHT)) * 0.5f,
		ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup(s_file_selector_title.c_str());

	FileSelectorItem* selected = nullptr;

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, UIPrimaryTextColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, UIPrimaryDarkColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UIPrimaryColor);

	bool is_open = !WantsToCloseMenu();
	bool directory_selected = false;
	if (ImGui::BeginPopupModal(
			s_file_selector_title.c_str(), &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleColor(ImGuiCol_Text, UIBackgroundTextColor);

		BeginMenuButtons();
		ResetFocusHere();

		if (!s_file_selector_current_directory.empty())
		{
			MenuButton(fmt::format(ICON_FA_FOLDER_OPEN " {}", s_file_selector_current_directory).c_str(), nullptr, false,
				LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY);
		}

		if (s_file_selector_directory && !s_file_selector_current_directory.empty())
		{
			if (MenuButton(ICON_FA_FOLDER_PLUS " <Use This Directory>", nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
				directory_selected = true;
		}

		for (FileSelectorItem& item : s_file_selector_items)
		{
			if (MenuButton(item.display_name.c_str(), nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
				selected = &item;
		}

		EndMenuButtons();

		ImGui::PopStyleColor(1);

		ImGui::EndPopup();
	}
	else
	{
		is_open = false;
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(3);
	ImGui::PopFont();

	if (is_open)
		GetFileSelectorHelpText(s_fullscreen_footer_text);

	if (selected)
	{
		if (selected->is_file)
		{
			s_file_selector_callback(selected->full_path);
			QueueResetFocus();
		}
		else
		{
			SetFileSelectorDirectory(std::move(selected->full_path));
		}
	}
	else if (directory_selected)
	{
		s_file_selector_callback(s_file_selector_current_directory);
	}
	else if (!is_open)
	{
		std::string no_path;
		s_file_selector_callback(no_path);
		CloseFileSelector();
	}
	else
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsKeyPressed(ImGuiKey_NavGamepadMenu, false))
		{
			if (!s_file_selector_items.empty() && s_file_selector_items.front().display_name == ICON_FA_FOLDER_OPEN
													  "  <Parent Directory>")
			{
				SetFileSelectorDirectory(std::move(s_file_selector_items.front().full_path));
				QueueResetFocus();
			}
		}
	}
}

bool ImGuiFullscreen::IsChoiceDialogOpen()
{
	return s_choice_dialog_open;
}

void ImGuiFullscreen::OpenChoiceDialog(std::string_view title, bool checkable, ChoiceDialogOptions options, ChoiceDialogCallback callback)
{
	if (s_choice_dialog_open)
		CloseChoiceDialog();

	s_choice_dialog_open = true;
	s_choice_dialog_checkable = checkable;
	s_choice_dialog_title = fmt::format("{}##choice_dialog", title);
	s_choice_dialog_options = std::move(options);
	s_choice_dialog_callback = std::move(callback);
	QueueResetFocus();
}

void ImGuiFullscreen::CloseChoiceDialog()
{
	if (!s_choice_dialog_open)
		return;

	s_choice_dialog_open = false;
	s_choice_dialog_checkable = false;
	std::string().swap(s_choice_dialog_title);
	ChoiceDialogOptions().swap(s_choice_dialog_options);
	ChoiceDialogCallback().swap(s_choice_dialog_callback);
	QueueResetFocus();
}

void ImGuiFullscreen::DrawChoiceDialog()
{
	if (!s_choice_dialog_open)
		return;

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, UIPrimaryTextColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, UIPrimaryDarkColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UIPrimaryColor);

	const float width = LayoutScale(600.0f);
	const float title_height = g_large_font->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetStyle().WindowPadding.y * 2.0f;
	const float height = std::min(LayoutScale(480.0f), title_height + (LayoutScale(LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY) +
																		  LayoutScale(LAYOUT_MENU_BUTTON_Y_PADDING) * 2.0f) *
																		  static_cast<float>(s_choice_dialog_options.size()));
	ImGui::SetNextWindowSize(ImVec2(width, height));
	ImGui::SetNextWindowPos((ImGui::GetIO().DisplaySize - LayoutScale(0.0f, LAYOUT_FOOTER_HEIGHT)) * 0.5f,
		ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup(s_choice_dialog_title.c_str());

	bool is_open = !WantsToCloseMenu();
	s32 choice = -1;

	if (ImGui::BeginPopupModal(
			s_choice_dialog_title.c_str(), &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::PushStyleColor(ImGuiCol_Text, UIBackgroundTextColor);

		BeginMenuButtons();
		ResetFocusHere();

		if (s_choice_dialog_checkable)
		{
			for (s32 i = 0; i < static_cast<s32>(s_choice_dialog_options.size()); i++)
			{
				auto& option = s_choice_dialog_options[i];

				const std::string title(fmt::format("{0} {1}", option.second ? ICON_FA_CHECK_SQUARE : ICON_FA_SQUARE, option.first));
				if (MenuButton(title.c_str(), nullptr, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
				{
					choice = i;
					option.second = !option.second;
				}
			}
		}
		else
		{
			for (s32 i = 0; i < static_cast<s32>(s_choice_dialog_options.size()); i++)
			{
				auto& option = s_choice_dialog_options[i];
				std::string title;
				if (option.second)
					title += ICON_FA_CHECK " ";
				title += option.first;

				if (ActiveButton(title.c_str(), option.second, true, LAYOUT_MENU_BUTTON_HEIGHT_NO_SUMMARY))
				{
					choice = i;
					for (s32 j = 0; j < static_cast<s32>(s_choice_dialog_options.size()); j++)
						s_choice_dialog_options[j].second = (j == i);
				}
			}
		}

		EndMenuButtons();

		ImGui::PopStyleColor(1);

		ImGui::EndPopup();
	}
	else
	{
		is_open = false;
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(3);
	ImGui::PopFont();

	if (choice >= 0)
	{
		const auto& option = s_choice_dialog_options[choice];
		s_choice_dialog_callback(choice, option.first, option.second);
	}
	else if (!is_open)
	{
		std::string no_string;
		s_choice_dialog_callback(-1, no_string, false);
		CloseChoiceDialog();
	}
	else
	{
		GetChoiceDialogHelpText(s_fullscreen_footer_text);
	}
}


bool ImGuiFullscreen::IsInputDialogOpen()
{
	return s_input_dialog_open;
}

void ImGuiFullscreen::OpenInputStringDialog(
	std::string title, std::string message, std::string caption, std::string ok_button_text, InputStringDialogCallback callback)
{
	s_input_dialog_open = true;
	s_input_dialog_title = std::move(title);
	s_input_dialog_message = std::move(message);
	s_input_dialog_caption = std::move(caption);
	s_input_dialog_ok_text = std::move(ok_button_text);
	s_input_dialog_callback = std::move(callback);
	QueueResetFocus();
}

void ImGuiFullscreen::DrawInputDialog()
{
	if (!s_input_dialog_open)
		return;

	ImGui::SetNextWindowSize(LayoutScale(700.0f, 0.0f));
	ImGui::SetNextWindowPos((ImGui::GetIO().DisplaySize - LayoutScale(0.0f, LAYOUT_FOOTER_HEIGHT)) * 0.5f,
		ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup(s_input_dialog_title.c_str());

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, UIPrimaryTextColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, UIPrimaryDarkColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UIPrimaryColor);

	bool is_open = true;
	if (ImGui::BeginPopupModal(s_input_dialog_title.c_str(), &is_open,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ResetFocusHere();
		ImGui::TextWrapped("%s", s_input_dialog_message.c_str());

		BeginMenuButtons();

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));

		if (!s_input_dialog_caption.empty())
		{
			const float prev = ImGui::GetCursorPosX();
			ImGui::TextUnformatted(s_input_dialog_caption.c_str());
			ImGui::SetNextItemWidth(ImGui::GetCursorPosX() - prev);
		}
		else
		{
			ImGui::SetNextItemWidth(ImGui::GetCurrentWindow()->WorkRect.GetWidth());
		}
		ImGui::InputText("##input", &s_input_dialog_text);

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(10.0f));

		const bool ok_enabled = !s_input_dialog_text.empty();

		if (ActiveButton(s_input_dialog_ok_text.c_str(), false, ok_enabled) && ok_enabled)
		{
			// have to move out in case they open another dialog in the callback
			InputStringDialogCallback cb(std::move(s_input_dialog_callback));
			std::string text(std::move(s_input_dialog_text));
			CloseInputDialog();
			ImGui::CloseCurrentPopup();
			cb(std::move(text));
		}

		if (ActiveButton(ICON_FA_TIMES " Cancel", false))
		{
			CloseInputDialog();

			ImGui::CloseCurrentPopup();
		}

		EndMenuButtons();

		ImGui::EndPopup();
	}
	if (!is_open)
		CloseInputDialog();
	else
		GetInputDialogHelpText(s_fullscreen_footer_text);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(3);
	ImGui::PopFont();
}

void ImGuiFullscreen::CloseInputDialog()
{
	if (!s_input_dialog_open)
		return;

	s_input_dialog_open = false;
	s_input_dialog_title = {};
	s_input_dialog_message = {};
	s_input_dialog_caption = {};
	s_input_dialog_ok_text = {};
	s_input_dialog_text = {};
	s_input_dialog_callback = {};
}

bool ImGuiFullscreen::IsMessageBoxDialogOpen()
{
	return s_message_dialog_open;
}

void ImGuiFullscreen::OpenConfirmMessageDialog(
	std::string title, std::string message, ConfirmMessageDialogCallback callback, std::string yes_button_text, std::string no_button_text)
{
	CloseMessageDialog();

	s_message_dialog_open = true;
	s_message_dialog_title = std::move(title);
	s_message_dialog_message = std::move(message);
	s_message_dialog_callback = std::move(callback);
	s_message_dialog_buttons[0] = std::move(yes_button_text);
	s_message_dialog_buttons[1] = std::move(no_button_text);
	QueueResetFocus();
}

void ImGuiFullscreen::OpenInfoMessageDialog(
	std::string title, std::string message, InfoMessageDialogCallback callback, std::string button_text)
{
	CloseMessageDialog();

	s_message_dialog_open = true;
	s_message_dialog_title = std::move(title);
	s_message_dialog_message = std::move(message);
	s_message_dialog_callback = std::move(callback);
	s_message_dialog_buttons[0] = std::move(button_text);
	QueueResetFocus();
}

void ImGuiFullscreen::OpenMessageDialog(std::string title, std::string message, MessageDialogCallback callback,
	std::string first_button_text, std::string second_button_text, std::string third_button_text)
{
	CloseMessageDialog();

	s_message_dialog_open = true;
	s_message_dialog_title = std::move(title);
	s_message_dialog_message = std::move(message);
	s_message_dialog_callback = std::move(callback);
	s_message_dialog_buttons[0] = std::move(first_button_text);
	s_message_dialog_buttons[1] = std::move(second_button_text);
	s_message_dialog_buttons[2] = std::move(third_button_text);
	QueueResetFocus();
}

void ImGuiFullscreen::CloseMessageDialog()
{
	if (!s_message_dialog_open)
		return;

	s_message_dialog_open = false;
	s_message_dialog_title = {};
	s_message_dialog_message = {};
	s_message_dialog_buttons = {};
	s_message_dialog_callback = {};
	QueueResetFocus();
}

void ImGuiFullscreen::DrawMessageDialog()
{
	if (!s_message_dialog_open)
		return;

	const char* win_id = s_message_dialog_title.empty() ? "##messagedialog" : s_message_dialog_title.c_str();

	ImGui::SetNextWindowSize(LayoutScale(700.0f, 0.0f));
	ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::OpenPopup(win_id);

	ImGui::PushFont(g_large_font);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(20.0f, 20.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, LayoutScale(10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, LayoutScale(LAYOUT_MENU_BUTTON_X_PADDING, LAYOUT_MENU_BUTTON_Y_PADDING));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, UIPrimaryTextColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, UIPrimaryDarkColor);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UIPrimaryColor);

	bool is_open = true;
	const u32 flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
					  (s_message_dialog_title.empty() ? ImGuiWindowFlags_NoTitleBar : 0);
	std::optional<s32> result;

	if (ImGui::BeginPopupModal(win_id, &is_open, flags))
	{
		BeginMenuButtons();
		ResetFocusHere();

		ImGui::TextWrapped("%s", s_message_dialog_message.c_str());
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + LayoutScale(20.0f));

		for (s32 button_index = 0; button_index < static_cast<s32>(s_message_dialog_buttons.size()); button_index++)
		{
			if (!s_message_dialog_buttons[button_index].empty() && ActiveButton(s_message_dialog_buttons[button_index].c_str(), false))
			{
				result = button_index;
				ImGui::CloseCurrentPopup();
			}
		}

		EndMenuButtons();

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(4);
	ImGui::PopFont();

	if (!is_open || result.has_value())
	{
		// have to move out in case they open another dialog in the callback
		auto cb = (std::move(s_message_dialog_callback));
		CloseMessageDialog();

		if (std::holds_alternative<InfoMessageDialogCallback>(cb))
		{
			const InfoMessageDialogCallback& func = std::get<InfoMessageDialogCallback>(cb);
			if (func)
				func();
		}
		else if (std::holds_alternative<ConfirmMessageDialogCallback>(cb))
		{
			const ConfirmMessageDialogCallback& func = std::get<ConfirmMessageDialogCallback>(cb);
			if (func)
				func(result.value_or(1) == 0);
		}
	}
	else
	{
		GetChoiceDialogHelpText(s_fullscreen_footer_text);
	}
}

static float s_notification_vertical_position = 0.15f;
static float s_notification_vertical_direction = 1.0f;

float ImGuiFullscreen::GetNotificationVerticalPosition()
{
	return s_notification_vertical_position;
}

float ImGuiFullscreen::GetNotificationVerticalDirection()
{
	return s_notification_vertical_direction;
}

void ImGuiFullscreen::SetNotificationVerticalPosition(float position, float direction)
{
	s_notification_vertical_position = position;
	s_notification_vertical_direction = direction;
}

ImGuiID ImGuiFullscreen::GetBackgroundProgressID(const char* str_id)
{
	return ImHashStr(str_id);
}

void ImGuiFullscreen::OpenBackgroundProgressDialog(const char* str_id, std::string message, s32 min, s32 max, s32 value)
{
	const ImGuiID id = GetBackgroundProgressID(str_id);

	std::unique_lock<std::mutex> lock(s_background_progress_lock);

#ifdef PCSX2_DEVBUILD
	for (const BackgroundProgressDialogData& data : s_background_progress_dialogs)
	{
		pxAssertMsg(data.id != id, "Duplicate background progress dialog open");
	}
#endif

	BackgroundProgressDialogData data;
	data.id = id;
	data.message = std::move(message);
	data.min = min;
	data.max = max;
	data.value = value;
	s_background_progress_dialogs.push_back(std::move(data));
}

void ImGuiFullscreen::UpdateBackgroundProgressDialog(const char* str_id, std::string message, s32 min, s32 max, s32 value)
{
	const ImGuiID id = GetBackgroundProgressID(str_id);

	std::unique_lock<std::mutex> lock(s_background_progress_lock);

	for (BackgroundProgressDialogData& data : s_background_progress_dialogs)
	{
		if (data.id == id)
		{
			data.message = std::move(message);
			data.min = min;
			data.max = max;
			data.value = value;
			return;
		}
	}

	pxFailRel("Updating unknown progress entry.");
}

void ImGuiFullscreen::CloseBackgroundProgressDialog(const char* str_id)
{
	const ImGuiID id = GetBackgroundProgressID(str_id);

	std::unique_lock<std::mutex> lock(s_background_progress_lock);

	for (auto it = s_background_progress_dialogs.begin(); it != s_background_progress_dialogs.end(); ++it)
	{
		if (it->id == id)
		{
			s_background_progress_dialogs.erase(it);
			return;
		}
	}

	pxFailRel("Closing unknown progress entry.");
}

void ImGuiFullscreen::DrawBackgroundProgressDialogs(ImVec2& position, float spacing)
{
	std::unique_lock<std::mutex> lock(s_background_progress_lock);
	if (s_background_progress_dialogs.empty())
		return;

	const float window_width = LayoutScale(500.0f);
	const float window_height = LayoutScale(75.0f);

	ImGui::PushStyleColor(ImGuiCol_WindowBg, UIPrimaryDarkColor);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, UISecondaryStrongColor);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, LayoutScale(4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, LayoutScale(1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, LayoutScale(10.0f, 10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, LayoutScale(10.0f, 10.0f));
	ImGui::PushFont(g_medium_font);

	ImDrawList* dl = ImGui::GetForegroundDrawList();

	for (const BackgroundProgressDialogData& data : s_background_progress_dialogs)
	{
		const float window_pos_x = position.x;
		const float window_pos_y = position.y - ((s_notification_vertical_direction < 0.0f) ? window_height : 0.0f);

		dl->AddRectFilled(ImVec2(window_pos_x, window_pos_y), ImVec2(window_pos_x + window_width, window_pos_y + window_height),
			IM_COL32(0x11, 0x11, 0x11, 200), LayoutScale(10.0f));

		ImVec2 pos(window_pos_x + LayoutScale(10.0f), window_pos_y + LayoutScale(10.0f));
		dl->AddText(g_medium_font, g_medium_font->FontSize, pos, IM_COL32(255, 255, 255, 255), data.message.c_str(), nullptr, 0.0f);
		pos.y += g_medium_font->FontSize + LayoutScale(10.0f);

		const ImVec2 box_end(pos.x + window_width - LayoutScale(10.0f * 2.0f), pos.y + LayoutScale(25.0f));
		dl->AddRectFilled(pos, box_end, ImGui::GetColorU32(UIPrimaryDarkColor));

		if (data.min != data.max)
		{
			const float fraction = static_cast<float>(data.value - data.min) / static_cast<float>(data.max - data.min);
			dl->AddRectFilled(pos, ImVec2(pos.x + fraction * (box_end.x - pos.x), box_end.y), ImGui::GetColorU32(UISecondaryColor));

			const std::string text(fmt::format("{}%", static_cast<int>(std::round(fraction * 100.0f))));
			const ImVec2 text_size(ImGui::CalcTextSize(text.c_str()));
			const ImVec2 text_pos(
				pos.x + ((box_end.x - pos.x) / 2.0f) - (text_size.x / 2.0f), pos.y + ((box_end.y - pos.y) / 2.0f) - (text_size.y / 2.0f));
			dl->AddText(g_medium_font, g_medium_font->FontSize, text_pos, ImGui::GetColorU32(UIPrimaryTextColor), text.c_str());
		}
		else
		{
			// indeterminate, so draw a scrolling bar
			const float bar_width = LayoutScale(30.0f);
			const float fraction = std::fmod(ImGui::GetTime(), 2.0f) * 0.5f;
			const ImVec2 bar_start(pos.x + ImLerp(0.0f, box_end.x, fraction) - bar_width, pos.y);
			const ImVec2 bar_end(std::min(bar_start.x + bar_width, box_end.x), pos.y + LayoutScale(25.0f));
			dl->AddRectFilled(ImClamp(bar_start, pos, box_end), ImClamp(bar_end, pos, box_end), ImGui::GetColorU32(UISecondaryColor));
		}

		position.y += s_notification_vertical_direction * (window_height + spacing);
	}

	ImGui::PopFont();
	ImGui::PopStyleVar(4);
	ImGui::PopStyleColor(2);
}

//////////////////////////////////////////////////////////////////////////
// Notifications
//////////////////////////////////////////////////////////////////////////

void ImGuiFullscreen::AddNotification(std::string key, float duration, std::string title, std::string text,
	std::string image_path)
{
	const Common::Timer::Value current_time = Common::Timer::GetCurrentValue();

	if (!key.empty())
	{
		for (auto it = s_notifications.begin(); it != s_notifications.end(); ++it)
		{
			if (it->key == key)
			{
				it->duration = duration;
				it->title = std::move(title);
				it->text = std::move(text);
				it->badge_path = std::move(image_path);

				// Don't fade it in again
				const float time_passed =
					static_cast<float>(Common::Timer::ConvertValueToSeconds(current_time - it->start_time));
				it->start_time =
					current_time - Common::Timer::ConvertSecondsToValue(std::min(time_passed, NOTIFICATION_FADE_IN_TIME));
				return;
			}
		}
	}

	Notification notif;
	notif.key = std::move(key);
	notif.duration = duration;
	notif.title = std::move(title);
	notif.text = std::move(text);
	notif.badge_path = std::move(image_path);
	notif.start_time = current_time;
	notif.move_time = current_time;
	notif.target_y = -1.0f;
	notif.last_y = -1.0f;
	s_notifications.push_back(std::move(notif));
}

void ImGuiFullscreen::ClearNotifications()
{
	s_notifications.clear();
}

void ImGuiFullscreen::DrawNotifications(ImVec2& position, float spacing)
{
	if (s_notifications.empty())
		return;

	static constexpr float MOVE_DURATION = 0.5f;
	const Common::Timer::Value current_time = Common::Timer::GetCurrentValue();

	const float horizontal_padding = ImGuiFullscreen::LayoutScale(20.0f);
	const float vertical_padding = ImGuiFullscreen::LayoutScale(10.0f);
	const float horizontal_spacing = ImGuiFullscreen::LayoutScale(10.0f);
	const float vertical_spacing = ImGuiFullscreen::LayoutScale(4.0f);
	const float badge_size = ImGuiFullscreen::LayoutScale(48.0f);
	const float min_width = ImGuiFullscreen::LayoutScale(200.0f);
	const float max_width = ImGuiFullscreen::LayoutScale(800.0f);
	const float max_text_width = max_width - badge_size - (horizontal_padding * 2.0f) - horizontal_spacing;
	const float min_height = (vertical_padding * 2.0f) + badge_size;
	const float shadow_size = ImGuiFullscreen::LayoutScale(4.0f);
	const float rounding = ImGuiFullscreen::LayoutScale(4.0f);

	ImFont* const title_font = ImGuiFullscreen::g_large_font;
	ImFont* const text_font = ImGuiFullscreen::g_medium_font;

	const u32 toast_background_color = s_light_theme ? IM_COL32(241, 241, 241, 255) : IM_COL32(0x21, 0x21, 0x21, 255);
	const u32 toast_border_color = s_light_theme ? IM_COL32(0x88, 0x88, 0x88, 255) : IM_COL32(0x48, 0x48, 0x48, 255);
	const u32 toast_title_color = s_light_theme ? IM_COL32(1, 1, 1, 255) : IM_COL32(0xff, 0xff, 0xff, 255);
	const u32 toast_text_color = s_light_theme ? IM_COL32(0, 0, 0, 255) : IM_COL32(0xff, 0xff, 0xff, 255);

	for (u32 index = 0; index < static_cast<u32>(s_notifications.size());)
	{
		Notification& notif = s_notifications[index];
		const float time_passed = static_cast<float>(Common::Timer::ConvertValueToSeconds(current_time - notif.start_time));
		if (time_passed >= notif.duration)
		{
			s_notifications.erase(s_notifications.begin() + index);
			continue;
		}

		const ImVec2 title_size(text_font->CalcTextSizeA(title_font->FontSize, max_text_width, max_text_width,
			notif.title.c_str(), notif.title.c_str() + notif.title.size()));

		const ImVec2 text_size(text_font->CalcTextSizeA(text_font->FontSize, max_text_width, max_text_width,
			notif.text.c_str(), notif.text.c_str() + notif.text.size()));

		const float box_width = std::max(
			(horizontal_padding * 2.0f) + badge_size + horizontal_spacing + std::max(title_size.x, text_size.x), min_width);
		const float box_height =
			std::max((vertical_padding * 2.0f) + title_size.y + vertical_spacing + text_size.y, min_height);

		u8 opacity;
		if (time_passed < NOTIFICATION_FADE_IN_TIME)
			opacity = static_cast<u8>((time_passed / NOTIFICATION_FADE_IN_TIME) * 255.0f);
		else if (time_passed > (notif.duration - NOTIFICATION_FADE_OUT_TIME))
			opacity = static_cast<u8>(std::min((notif.duration - time_passed) / NOTIFICATION_FADE_OUT_TIME, 1.0f) * 255.0f);
		else
			opacity = 255;

		const float expected_y = position.y - ((s_notification_vertical_direction < 0.0f) ? box_height : 0.0f);
		float actual_y = notif.last_y;
		if (notif.target_y != expected_y)
		{
			notif.move_time = current_time;
			notif.target_y = expected_y;
			notif.last_y = (notif.last_y < 0.0f) ? expected_y : notif.last_y;
			actual_y = notif.last_y;
		}
		else if (actual_y != expected_y)
		{
			const float time_since_move =
				static_cast<float>(Common::Timer::ConvertValueToSeconds(current_time - notif.move_time));
			if (time_since_move >= MOVE_DURATION)
			{
				notif.move_time = current_time;
				notif.last_y = notif.target_y;
				actual_y = notif.last_y;
			}
			else
			{
				const float frac = Easing::OutExpo(time_since_move / MOVE_DURATION);
				actual_y = notif.last_y - ((notif.last_y - notif.target_y) * frac);
			}
		}

		const ImVec2 box_min(position.x, actual_y);
		const ImVec2 box_max(box_min.x + box_width, box_min.y + box_height);
		const u32 background_color = (toast_background_color & ~IM_COL32_A_MASK) | (opacity << IM_COL32_A_SHIFT);
		const u32 border_color = (toast_border_color & ~IM_COL32_A_MASK) | (opacity << IM_COL32_A_SHIFT);

		ImDrawList* dl = ImGui::GetForegroundDrawList();
		dl->AddRectFilled(ImVec2(box_min.x + shadow_size, box_min.y + shadow_size),
			ImVec2(box_max.x + shadow_size, box_max.y + shadow_size),
			IM_COL32(20, 20, 20, (180 * opacity) / 255u), rounding, ImDrawFlags_RoundCornersAll);
		dl->AddRectFilled(box_min, box_max, background_color, rounding, ImDrawFlags_RoundCornersAll);
		dl->AddRect(box_min, box_max, border_color, rounding, ImDrawFlags_RoundCornersAll, ImGuiFullscreen::LayoutScale(1.0f));

		const ImVec2 badge_min(box_min.x + horizontal_padding, box_min.y + vertical_padding);
		const ImVec2 badge_max(badge_min.x + badge_size, badge_min.y + badge_size);
		if (!notif.badge_path.empty())
		{
			GSTexture* tex = GetCachedTexture(notif.badge_path.c_str());
			if (tex)
			{
				dl->AddImage(tex->GetNativeHandle(), badge_min, badge_max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
					IM_COL32(255, 255, 255, opacity));
			}
		}

		const ImVec2 title_min(badge_max.x + horizontal_spacing, box_min.y + vertical_padding);
		const ImVec2 title_max(title_min.x + title_size.x, title_min.y + title_size.y);
		const u32 title_col = (toast_title_color & ~IM_COL32_A_MASK) | (opacity << IM_COL32_A_SHIFT);
		dl->AddText(title_font, title_font->FontSize, title_min, title_col, notif.title.c_str(),
			notif.title.c_str() + notif.title.size(), max_text_width);

		const ImVec2 text_min(badge_max.x + horizontal_spacing, title_max.y + vertical_spacing);
		const ImVec2 text_max(text_min.x + text_size.x, text_min.y + text_size.y);
		const u32 text_col = (toast_text_color & ~IM_COL32_A_MASK) | (opacity << IM_COL32_A_SHIFT);
		dl->AddText(text_font, text_font->FontSize, text_min, text_col, notif.text.c_str(),
			notif.text.c_str() + notif.text.size(), max_text_width);

		position.y += s_notification_vertical_direction * (box_height + shadow_size + spacing);
		index++;
	}
}

void ImGuiFullscreen::ShowToast(std::string title, std::string message, float duration)
{
	s_toast_title = std::move(title);
	s_toast_message = std::move(message);
	s_toast_start_time = Common::Timer::GetCurrentValue();
	s_toast_duration = duration;
}

void ImGuiFullscreen::ClearToast()
{
	s_toast_message = {};
	s_toast_title = {};
	s_toast_start_time = 0;
	s_toast_duration = 0.0f;
}

void ImGuiFullscreen::DrawToast()
{
	if (s_toast_title.empty() && s_toast_message.empty())
		return;

	const float elapsed = static_cast<float>(Common::Timer::ConvertValueToSeconds(Common::Timer::GetCurrentValue() - s_toast_start_time));
	if (elapsed >= s_toast_duration)
	{
		ClearToast();
		return;
	}

	// fade out the last second
	const float alpha = std::min(std::min(elapsed * 4.0f, s_toast_duration - elapsed), 1.0f);

	const float max_width = LayoutScale(600.0f);

	ImFont* title_font = g_large_font;
	ImFont* message_font = g_medium_font;
	const float padding = LayoutScale(20.0f);
	const float total_padding = padding * 2.0f;
	const float margin = LayoutScale(20.0f + (s_fullscreen_footer_text.empty() ? 0.0f : LAYOUT_FOOTER_HEIGHT));
	const float spacing = s_toast_title.empty() ? 0.0f : LayoutScale(10.0f);
	const ImVec2 display_size(ImGui::GetIO().DisplaySize);
	const ImVec2 title_size(s_toast_title.empty() ? ImVec2(0.0f, 0.0f) :
													title_font->CalcTextSizeA(title_font->FontSize, FLT_MAX, max_width,
														s_toast_title.c_str(), s_toast_title.c_str() + s_toast_title.length()));
	const ImVec2 message_size(s_toast_message.empty() ? ImVec2(0.0f, 0.0f) :
														message_font->CalcTextSizeA(message_font->FontSize, FLT_MAX, max_width,
															s_toast_message.c_str(), s_toast_message.c_str() + s_toast_message.length()));
	const ImVec2 comb_size(std::max(title_size.x, message_size.x), title_size.y + spacing + message_size.y);

	const ImVec2 box_size(comb_size.x + total_padding, comb_size.y + total_padding);
	const ImVec2 box_pos((display_size.x - box_size.x) * 0.5f, (display_size.y - margin - box_size.y));

	ImDrawList* dl = ImGui::GetForegroundDrawList();
	dl->AddRectFilled(box_pos, box_pos + box_size, ImGui::GetColorU32(ModAlpha(UIPrimaryColor, alpha)), padding);
	if (!s_toast_title.empty())
	{
		const float offset = (comb_size.x - title_size.x) * 0.5f;
		dl->AddText(title_font, title_font->FontSize, box_pos + ImVec2(offset + padding, padding),
			ImGui::GetColorU32(ModAlpha(UIPrimaryTextColor, alpha)), s_toast_title.c_str(), s_toast_title.c_str() + s_toast_title.length(),
			max_width);
	}
	if (!s_toast_message.empty())
	{
		const float offset = (comb_size.x - message_size.x) * 0.5f;
		dl->AddText(message_font, message_font->FontSize, box_pos + ImVec2(offset + padding, padding + spacing + title_size.y),
			ImGui::GetColorU32(ModAlpha(UIPrimaryTextColor, alpha)), s_toast_message.c_str(),
			s_toast_message.c_str() + s_toast_message.length(), max_width);
	}
}

void ImGuiFullscreen::SetTheme(bool light)
{
	s_light_theme = light;

	if (!light)
	{
		// dark
		UIBackgroundColor = HEX_TO_IMVEC4(0x212121, 0xff);
		UIBackgroundTextColor = HEX_TO_IMVEC4(0xffffff, 0xff);
		UIBackgroundLineColor = HEX_TO_IMVEC4(0xf0f0f0, 0xff);
		UIBackgroundHighlightColor = HEX_TO_IMVEC4(0x4b4b4b, 0xff);
		UIPopupBackgroundColor = HEX_TO_IMVEC4(0x212121, 0xf2);
		UIPrimaryColor = HEX_TO_IMVEC4(0x2e2e2e, 0xff);
		UIPrimaryLightColor = HEX_TO_IMVEC4(0x484848, 0xff);
		UIPrimaryDarkColor = HEX_TO_IMVEC4(0x000000, 0xff);
		UIPrimaryTextColor = HEX_TO_IMVEC4(0xffffff, 0xff);
		UIDisabledColor = HEX_TO_IMVEC4(0xaaaaaa, 0xff);
		UITextHighlightColor = HEX_TO_IMVEC4(0x90caf9, 0xff);
		UIPrimaryLineColor = HEX_TO_IMVEC4(0xffffff, 0xff);
		UISecondaryColor = HEX_TO_IMVEC4(0x0d47a1, 0xff);
		UISecondaryStrongColor = HEX_TO_IMVEC4(0x63a4ff, 0xff);
		UISecondaryWeakColor = HEX_TO_IMVEC4(0x002171, 0xff);
		UISecondaryTextColor = HEX_TO_IMVEC4(0xffffff, 0xff);
	}
	else
	{
		// light
		UIBackgroundColor = HEX_TO_IMVEC4(0xc8c8c8, 0xff);
		UIBackgroundTextColor = HEX_TO_IMVEC4(0x000000, 0xff);
		UIBackgroundLineColor = HEX_TO_IMVEC4(0xe1e2e1, 0xff);
		UIBackgroundHighlightColor = HEX_TO_IMVEC4(0xe1e2e1, 0xff);
		UIPopupBackgroundColor = HEX_TO_IMVEC4(0xd8d8d8, 0xf2);
		UIPrimaryColor = HEX_TO_IMVEC4(0x2a3e78, 0xff);
		UIPrimaryLightColor = HEX_TO_IMVEC4(0x235cd9, 0xff);
		UIPrimaryDarkColor = HEX_TO_IMVEC4(0x1d2953, 0xff);
		UIPrimaryTextColor = HEX_TO_IMVEC4(0xffffff, 0xff);
		UIDisabledColor = HEX_TO_IMVEC4(0x999999, 0xff);
		UITextHighlightColor = HEX_TO_IMVEC4(0x8e8e8e, 0xff);
		UIPrimaryLineColor = HEX_TO_IMVEC4(0x000000, 0xff);
		UISecondaryColor = HEX_TO_IMVEC4(0x2a3e78, 0xff);
		UISecondaryStrongColor = HEX_TO_IMVEC4(0x464db1, 0xff);
		UISecondaryWeakColor = HEX_TO_IMVEC4(0xc0cfff, 0xff);
		UISecondaryTextColor = HEX_TO_IMVEC4(0x000000, 0xff);
	}
}
