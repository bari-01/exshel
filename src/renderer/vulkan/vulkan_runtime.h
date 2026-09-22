#ifndef __VULKAN_RUNTIME_H_
#define __VULKAN_RUNTIME_H_

#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

typedef struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t graphics_family;
    uint32_t present_family;
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkSurfaceKHR surface;
    VkExtent2D extent;
    VkFormat swapchain_format;
    VkColorSpaceKHR swapchain_color_space;
    VkSwapchainKHR swapchain;
    uint32_t image_count;
    VkImage *images;
    VkImageView *image_views;
    VkCommandPool command_pool;
    VkCommandBuffer *command_buffers;
    VkSemaphore *image_available_sems;
    VkSemaphore *render_finished_sems;
    VkFence *in_flight_fences;
    uint32_t current_frame;

    // Text pipeline & atlas resources
    VkImage atlas_image;
    VkDeviceMemory atlas_memory;
    VkImageView atlas_view;
    VkSampler atlas_sampler;
    VkBuffer atlas_staging_buffer;
    VkDeviceMemory atlas_staging_memory;
    void *atlas_staging_mapped;
    bool atlas_initialized;

    VkDescriptorSetLayout text_desc_layout;
    VkDescriptorPool text_desc_pool;
    VkDescriptorSet text_desc_set;
    VkPipelineLayout text_pipeline_layout;
    VkPipeline text_pipeline;

    VkBuffer glyph_vertex_buffer;
    VkDeviceMemory glyph_vertex_memory;
    void *glyph_vertex_mapped;

    bool initialized;
} VulkanContext;

typedef enum {
    DISPLAY_WAYLAND,
    DISPLAY_X11,
} DisplayType;

typedef struct RenderList RenderList;
typedef struct GlyphAtlas GlyphAtlas;

int vulkan_runtime_init(VulkanContext *vk, DisplayType display_type,
    void *native_display, void *native_window, uint32_t width, uint32_t height);
int vulkan_runtime_cleanup(VulkanContext *vk);
int vulkan_runtime_render_frame(
    VulkanContext *vk, const RenderList *render_list, GlyphAtlas *atlas);

#endif // __VULKAN_RUNTIME_H_
