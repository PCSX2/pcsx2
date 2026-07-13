/* Copyright (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro_vulkan.h)
 * ---------------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBRETRO_VULKAN_H__
#define LIBRETRO_VULKAN_H__

#include <libretro.h>
#include <vulkan/vulkan.h>

#define RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION 5
#define RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION 2

struct retro_vulkan_image
{
   VkImageView image_view;
   VkImageLayout image_layout;
   VkImageViewCreateInfo create_info;
};

typedef void (*retro_vulkan_set_image_t)(void *handle,
      const struct retro_vulkan_image *image,
      uint32_t num_semaphores,
      const VkSemaphore *semaphores,
      uint32_t src_queue_family);

typedef uint32_t (*retro_vulkan_get_sync_index_t)(void *handle);
typedef uint32_t (*retro_vulkan_get_sync_index_mask_t)(void *handle);
typedef void (*retro_vulkan_set_command_buffers_t)(void *handle,
      uint32_t num_cmd,
      const VkCommandBuffer *cmd);
typedef void (*retro_vulkan_wait_sync_index_t)(void *handle);
typedef void (*retro_vulkan_lock_queue_t)(void *handle);
typedef void (*retro_vulkan_unlock_queue_t)(void *handle);
typedef void (*retro_vulkan_set_signal_semaphore_t)(void *handle, VkSemaphore semaphore);

typedef const VkApplicationInfo *(*retro_vulkan_get_application_info_t)(void);

struct retro_vulkan_context
{
   VkPhysicalDevice gpu;
   VkDevice device;
   VkQueue queue;
   uint32_t queue_family_index;
   VkQueue presentation_queue;
   uint32_t presentation_queue_family_index;
};

/* This is only used in v1 of the negotiation interface.
 * It is deprecated since it cannot express PDF2 features or optional extensions. */
typedef bool (*retro_vulkan_create_device_t)(
      struct retro_vulkan_context *context,
      VkInstance instance,
      VkPhysicalDevice gpu,
      VkSurfaceKHR surface,
      PFN_vkGetInstanceProcAddr get_instance_proc_addr,
      const char **required_device_extensions,
      unsigned num_required_device_extensions,
      const char **required_device_layers,
      unsigned num_required_device_layers,
      const VkPhysicalDeviceFeatures *required_features);

typedef void (*retro_vulkan_destroy_device_t)(void);

/* v2 CONTEXT_NEGOTIATION_INTERFACE only. */
typedef VkInstance (*retro_vulkan_create_instance_wrapper_t)(
      void *opaque, const VkInstanceCreateInfo *create_info);

/* v2 CONTEXT_NEGOTIATION_INTERFACE only. */
typedef VkInstance (*retro_vulkan_create_instance_t)(
      PFN_vkGetInstanceProcAddr get_instance_proc_addr,
      const VkApplicationInfo *app,
      retro_vulkan_create_instance_wrapper_t create_instance_wrapper,
      void *opaque);

/* v2 CONTEXT_NEGOTIATION_INTERFACE only. */
typedef VkDevice (*retro_vulkan_create_device_wrapper_t)(
      VkPhysicalDevice gpu, void *opaque,
      const VkDeviceCreateInfo *create_info);

/* v2 CONTEXT_NEGOTIATION_INTERFACE only. */
typedef bool (*retro_vulkan_create_device2_t)(
      struct retro_vulkan_context *context,
      VkInstance instance,
      VkPhysicalDevice gpu,
      VkSurfaceKHR surface,
      PFN_vkGetInstanceProcAddr get_instance_proc_addr,
      retro_vulkan_create_device_wrapper_t create_device_wrapper,
      void *opaque);

/* Note on thread safety:
 * The Vulkan API is heavily designed around multi-threading, and
 * the libretro interface for it should also be threading friendly.
 * A core should be able to build command buffers and submit
 * command buffers to the GPU from any thread.
 */

struct retro_hw_render_context_negotiation_interface_vulkan
{
   /* Must be set to RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN. */
   enum retro_hw_render_context_negotiation_interface_type interface_type;
   /* Usually set to RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION,
    * but can be lower depending on GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT. */
   unsigned interface_version;

   /* If non-NULL, returns a VkApplicationInfo struct that the frontend can use instead of
    * its "default" application info.
    * VkApplicationInfo::apiVersion also controls the target core Vulkan version for instance level functionality.
    * Lifetime of the returned pointer must remain until the retro_vulkan_context is initialized.
    *
    * NOTE: For optimal compatibility with e.g. Android which is very slow to update its loader,
    * a core version of 1.1 should be requested. Features beyond that can be requested with extensions.
    * Vulkan 1.0 is only appropriate for legacy cores, but is still supported.
    * A frontend is free to bump the instance creation apiVersion as necessary if the frontend requires more advanced core features.
    *
    * v2: This function must not be NULL, and must not return NULL.
    * v1: It was not clearly defined if this function could return NULL.
    *     Frontends should be defensive and provide a default VkApplicationInfo
    *     if this function returns NULL or if this function is NULL.
    */
   retro_vulkan_get_application_info_t get_application_info;

   /* If non-NULL, the libretro core will choose one or more physical devices,
    * create one or more logical devices and create one or more queues.
    * The core must prepare a designated PhysicalDevice, Device, Queue and queue family index
    * which the frontend will use for its internal operation.
    *
    * If gpu is not VK_NULL_HANDLE, the physical device provided to the frontend must be this PhysicalDevice if the call succeeds.
    * The core is still free to use other physical devices for other purposes that are private to the core.
    *
    * The frontend will request certain extensions and layers for a device which is created.
    * The core must ensure that the queue and queue_family_index support GRAPHICS and COMPUTE.
    *
    * If surface is not VK_NULL_HANDLE, the core must consider presentation when creating the queues.
    * If presentation to "surface" is supported on the queue, presentation_queue must be equal to queue.
    * If not, a second queue must be provided in presentation_queue and presentation_queue_index.
    * If surface is not VK_NULL_HANDLE, the instance from frontend will have been created with supported for
    * VK_KHR_surface extension.
    *
    * The core is free to set its own queue priorities.
    * Device provided to frontend is owned by the frontend, but any additional device resources must be freed by core
    * in destroy_device callback.
    *
    * If this function returns true, a PhysicalDevice, Device and Queues are initialized.
    * If false, none of the above have been initialized and the frontend will attempt
    * to fallback to "default" device creation, as if this function was never called.
    */
   retro_vulkan_create_device_t create_device;

   /* If non-NULL, this callback is called similar to context_destroy for HW_RENDER_INTERFACE.
    * However, it will be called even if context_reset was not called.
    * This can happen if the context never succeeds in being created.
    * destroy_device will always be called before the VkInstance
    * of the frontend is destroyed if create_device was called successfully so that the core has a chance of
    * tearing down its own device resources.
    *
    * Only auxillary resources should be freed here, i.e. resources which are not part of retro_vulkan_context.
    * v2: Auxillary instance resources created during create_instance can also be freed here.
    */
   retro_vulkan_destroy_device_t destroy_device;

   /* v2 API: If interface_version is < 2, fields below must be ignored.
    * If the frontend does not support interface version 2, the v1 entry points will be used instead. */

   /* If non-NULL, this is called to create an instance, otherwise a VkInstance is created by the frontend.
    * v1 interface bug: The only way to enable instance features is through core versions signalled in VkApplicationInfo.
    * The frontend may request that certain extensions and layers
    * are enabled on the VkInstance. Application may add additional features.
    * If app is non-NULL, apiVersion controls the minimum core version required by the application.
    * Return a VkInstance or VK_NULL_HANDLE. The VkInstance is owned by the frontend.
    *
    * Rather than call vkCreateInstance directly, a core must call the CreateInstance wrapper provided with:
    * VkInstance instance = create_instance_wrapper(opaque, &create_info);
    * If the core wishes to create a private instance for whatever reason (relying on shared memory for example),
    * it may call vkCreateInstance directly. */
   retro_vulkan_create_instance_t create_instance;

   /* If non-NULL and frontend recognizes negotiation interface >= 2, create_device2 takes precedence over create_device.
    * Similar to create_device, but is extended to better understand new core versions and PDF2 feature enablement.
    * Requirements for create_device2 are the same as create_device unless a difference is mentioned.
    *
    * v2 consideration:
    * If the chosen gpu by frontend cannot be supported, a core must return false.
    *
    * NOTE: "Cannot be supported" is intentionally vaguely defined.
    * Refusing to run on an iGPU for a very intensive core with desktop GPU as a minimum spec may be in the gray area.
    * Not supporting optional features is not a good reason to reject a physical device, however.
    *
    * On device creation feature with explicit gpu, a frontend should fall back create_device2 with gpu == VK_NULL_HANDLE and let core
    * decide on a supported device if possible.
    *
    * A core must assume that the explicitly provided GPU is the only guaranteed attempt it has to create a device.
    * A fallback may not be attempted if there are particular reasons why only a specific physical device can work,
    * but these situations should be esoteric and rare in nature, e.g. a libretro frontend is implemented with external memory
    * and only LUID matching would work.
    * Cores and frontends should ensure "best effort" when negotiating like this and appropriate logging is encouraged.
    *
    * v1 note: In the v1 version of create_device, it was never expected that create_device would fail like this,
    * and frontends are not expected to attempt fall backs.
    *
    * Rather than call vkCreateDevice directly, a core must call the CreateDevice wrapper provided with:
    * VkDevice device = create_device_wrapper(gpu, opaque, &create_info);
    * If the core wishes to create a private device for whatever reason (relying on shared memory for example),
    * it may call vkCreateDevice directly.
    *
    * This allows the frontend to add additional extensions that it requires as well as adjust the PDF2 pNext as required.
    * It is also possible adjust the queue create infos in case the frontend desires to allocate some private queues.
    *
    * The get_instance_proc_addr provided in create_device2 must be the same as create_instance.
    *
    * NOTE: The frontend must not disable features requested by application.
    * NOTE: The frontend must not add any robustness features as some API behavior may change (VK_EXT_descriptor_buffer comes to mind).
    * I.e. robustBufferAccess and the like. (nullDescriptor from robustness2 is allowed to be enabled).
    */
   retro_vulkan_create_device2_t create_device2;
};

struct retro_hw_render_interface_vulkan
{
   /* Must be set to RETRO_HW_RENDER_INTERFACE_VULKAN. */
   enum retro_hw_render_interface_type interface_type;
   /* Must be set to RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION. */
   unsigned interface_version;

   /* Opaque handle to the Vulkan backend in the frontend
    * which must be passed along to all function pointers
    * in this interface.
    *
    * The rationale for including a handle here (which libretro v1
    * doesn't currently do in general) is:
    *
    * - Vulkan cores should be able to be freely threaded without lots of fuzz.
    *   This would break frontends which currently rely on TLS
    *   to deal with multiple cores loaded at the same time.
    * - Fixing this in general is TODO for an eventual libretro v2.
    */
   void *handle;

   /* The Vulkan instance the context is using. */
   VkInstance instance;
   /* The physical device used. */
   VkPhysicalDevice gpu;
   /* The logical device used. */
   VkDevice device;

   /* Allows a core to fetch all its needed symbols without having to link
    * against the loader itself. */
   PFN_vkGetDeviceProcAddr get_device_proc_addr;
   PFN_vkGetInstanceProcAddr get_instance_proc_addr;

   /* The queue the core must use to submit data.
    * This queue and index must remain constant throughout the lifetime
    * of the context.
    *
    * This queue will be the queue that supports graphics and compute
    * if the device supports compute.
    */
   VkQueue queue;
   unsigned queue_index;

   /* Before calling retro_video_refresh_t with RETRO_HW_FRAME_BUFFER_VALID,
    * set which image to use for this frame.
    *
    * If num_semaphores is non-zero, the frontend will wait for the
    * semaphores provided to be signaled before using the results further
    * in the pipeline.
    *
    * Semaphores provided by a single call to set_image will only be
    * waited for once (waiting for a semaphore resets it).
    * E.g. set_image, video_refresh, and then another
    * video_refresh without set_image,
    * but same image will only wait for semaphores once.
    *
    * For this reason, ownership transfer will only occur if semaphores
    * are waited on for a particular frame in the frontend.
    *
    * Using semaphores is optional for synchronization purposes,
    * but if not using
    * semaphores, an image memory barrier in vkCmdPipelineBarrier
    * should be used in the graphics_queue.
    * Example:
    *
    * vkCmdPipelineBarrier(cmd,
    *    srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    *    dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    *    image_memory_barrier = {
    *       srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    *       dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    *    });
    *
    * The use of pipeline barriers instead of semaphores is encouraged
    * as it is simpler and more fine-grained. A layout transition
    * must generally happen anyways which requires a
    * pipeline barrier.
    *
    * The image passed to set_image must have imageUsage flags set to at least
    * VK_IMAGE_USAGE_TRANSFER_SRC_BIT and VK_IMAGE_USAGE_SAMPLED_BIT.
    * The core will naturally want to use flags such as
    * VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT and/or
    * VK_IMAGE_USAGE_TRANSFER_DST_BIT depending
    * on how the final image is created.
    *
    * The image must also have been created with MUTABLE_FORMAT bit set if
    * 8-bit formats are used, so that the frontend can reinterpret sRGB
    * formats as it sees fit.
    *
    * Images passed to set_image should be created with TILING_OPTIMAL.
    * The image layout should be transitioned to either
    * VK_IMAGE_LAYOUT_GENERIC or VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
    * The actual image layout used must be set in image_layout.
    *
    * The image must be a 2D texture which may or not be layered
    * and/or mipmapped.
    *
    * The image must be suitable for linear sampling.
    * While the image_view is typically the only field used,
    * the frontend may want to reinterpret the texture as sRGB vs.
    * non-sRGB for example so the VkImageViewCreateInfo used to
    * create the image view must also be passed in.
    *
    * The data in the pointer to the image struct will not be copied
    * as the pNext field in create_info cannot be reliably deep-copied.
    * The image pointer passed to set_image must be valid until
    * retro_video_refresh_t has returned.
    *
    * If frame duping is used when passing NULL to retro_video_refresh_t,
    * the frontend is free to either use the latest image passed to
    * set_image or reuse the older pointer passed to set_image the
    * frame RETRO_HW_FRAME_BUFFER_VALID was last used.
    *
    * Essentially, the lifetime of the pointer passed to
    * retro_video_refresh_t should be extended if frame duping is used
    * so that the frontend can reuse the older pointer.
    *
    * The image itself however, must not be touched by the core until
    * wait_sync_index has been completed later. The frontend may perform
    * layout transitions on the image, so even read-only access is not defined.
    * The exception to read-only rule is if GENERAL layout is used for the image.
    * In this case, the frontend is not allowed to perform any layout transitions,
    * so concurrent reads from core and frontend are allowed.
    *
    * If frame duping is used, or if set_command_buffers is used,
    * the frontend will not wait for any semaphores.
    *
    * The src_queue_family is used to specify which queue family
    * the image is currently owned by. If using multiple queue families
    * (e.g. async compute), the frontend will need to acquire ownership of the
    * image before rendering with it and release the image afterwards.
    *
    * If src_queue_family is equal to the queue family (queue_index),
    * no ownership transfer will occur.
    * Similarly, if src_queue_family is VK_QUEUE_FAMILY_IGNORED,
    * no ownership transfer will occur.
    *
    * The frontend will always release ownership back to src_queue_family.
    * Waiting for frontend to complete with wait_sync_index() ensures that
    * the frontend has released ownership back to the application.
    * Note that in Vulkan, transfering ownership is a two-part process.
    *
    * Example frame:
    *  - core releases ownership from src_queue_index to queue_index with VkImageMemoryBarrier.
    *  - core calls set_image with src_queue_index.
    *  - Frontend will acquire the image with src_queue_index -> queue_index as well, completing the ownership transfer.
    *  - Frontend renders the frame.
    *  - Frontend releases ownership with queue_index -> src_queue_index.
    *  - Next time image is used, core must acquire ownership from queue_index ...
    *
    * Since the frontend releases ownership, we cannot necessarily dupe the frame because
    * the core needs to make the roundtrip of ownership transfer.
    */
   retro_vulkan_set_image_t set_image;

   /* Get the current sync index for this frame which is obtained in
    * frontend by calling e.g. vkAcquireNextImageKHR before calling
    * retro_run().
    *
    * This index will correspond to which swapchain buffer is currently
    * the active one.
    *
    * Knowing this index is very useful for maintaining safe asynchronous CPU
    * and GPU operation without stalling.
    *
    * The common pattern for synchronization is to receive fences when
    * submitting command buffers to Vulkan (vkQueueSubmit) and add this fence
    * to a list of fences for frame number get_sync_index().
    *
    * Next time we receive the same get_sync_index(), we can wait for the
    * fences from before, which will usually return immediately as the
    * frontend will generally also avoid letting the GPU run ahead too much.
    *
    * After the fence has signaled, we know that the GPU has completed all
    * GPU work related to work submitted in the frame we last saw get_sync_index().
    *
    * This means we can safely reuse or free resources allocated in this frame.
    *
    * In theory, even if we wait for the fences correctly, it is not technically
    * safe to write to the image we earlier passed to the frontend since we're
    * not waiting for the frontend GPU jobs to complete.
    *
    * The frontend will guarantee that the appropriate pipeline barrier
    * in graphics_queue has been used such that
    * VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT cannot
    * start until the frontend is done with the image.
    */
   retro_vulkan_get_sync_index_t get_sync_index;

   /* Returns a bitmask of how many swapchain images we currently have
    * in the frontend.
    *
    * If bit #N is set in the return value, get_sync_index can return N.
    * Knowing this value is useful for preallocating per-frame management
    * structures ahead of time.
    *
    * While this value will typically remain constant throughout the
    * applications lifecycle, it may for example change if the frontend
    * suddently changes fullscreen state and/or latency.
    *
    * If this value ever changes, it is safe to assume that the device
    * is completely idle and all synchronization objects can be deleted
    * right away as desired.
    */
   retro_vulkan_get_sync_index_mask_t get_sync_index_mask;

   /* Instead of submitting the command buffer to the queue first, the core
    * can pass along its command buffer to the frontend, and the frontend
    * will submit the command buffer together with the frontends command buffers.
    *
    * This has the advantage that the overhead of vkQueueSubmit can be
    * amortized into a single call. For this mode, semaphores in set_image
    * will be ignored, so vkCmdPipelineBarrier must be used to synchronize
    * the core and frontend.
    *
    * The command buffers in set_command_buffers are only executed once,
    * even if frame duping is used.
    *
    * If frame duping is used, set_image should be used for the frames
    * which should be duped instead.
    *
    * Command buffers passed to the frontend with set_command_buffers
    * must not actually be submitted to the GPU until retro_video_refresh_t
    * is called.
    *
    * The frontend must submit the command buffer before submitting any
    * other command buffers provided by set_command_buffers. */
   retro_vulkan_set_command_buffers_t set_command_buffers;

   /* Waits on CPU for device activity for the current sync index to complete.
    * This is useful since the core will not have a relevant fence to sync with
    * when the frontend is submitting the command buffers. */
   retro_vulkan_wait_sync_index_t wait_sync_index;

   /* If the core submits command buffers itself to any of the queues provided
    * in this interface, the core must lock and unlock the frontend from
    * racing on the VkQueue.
    *
    * Queue submission can happen on any thread.
    * Even if queue submission happens on the same thread as retro_run(),
    * the lock/unlock functions must still be called.
    *
    * NOTE: Queue submissions are heavy-weight. */
   retro_vulkan_lock_queue_t lock_queue;
   retro_vulkan_unlock_queue_t unlock_queue;

   /* Sets a semaphore which is signaled when the image in set_image can safely be reused.
    * The semaphore is consumed next call to retro_video_refresh_t.
    * The semaphore will be signalled even for duped frames.
    * The semaphore will be signalled only once, so set_signal_semaphore should be called every frame.
    * The semaphore may be VK_NULL_HANDLE, which disables semaphore signalling for next call to retro_video_refresh_t.
    *
    * This is mostly useful to support use cases where you're rendering to a single image that
    * is recycled in a ping-pong fashion with the frontend to save memory (but potentially less throughput).
    */
   retro_vulkan_set_signal_semaphore_t set_signal_semaphore;
};

#endif
