#include "vulkan_runtime.h"
#include "../../font/glyph_atlas.h"
#include "../../widget/widget.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#ifdef USE_WAYLAND
#include <vulkan/vulkan_wayland.h>
#include <wayland-client.h>
#endif

#ifdef USE_X11
#include <xcb/xcb.h>
// load vulkan after xcb (this comment stops clang-format)
#include <vulkan/vulkan_xcb.h>
#endif
#include "../../utils.h"

static const uint32_t text_vert_spv[] =
#include "shaders/text_vert_spv.h"
    ;

static const uint32_t text_frag_spv[] =
#include "shaders/text_frag_spv.h"
    ;

typedef struct {
    float pos[2];
    float uv[2];
    float color[4];
} GlyphVertex;

static uint32_t
find_memory_type(VkPhysicalDevice physical_device, uint32_t type_filter,
    VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool
init_text_pipeline(VulkanContext *vk)
{
    uint32_t atlas_dim = 512;
    VkDeviceSize atlas_size = atlas_dim * atlas_dim;

    // create_atlas_image
    VkImageCreateInfo image_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .extent = {atlas_dim, atlas_dim, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    if (vkCreateImage(vk->device, &image_ci, NULL, &vk->atlas_image) !=
        VK_SUCCESS)
        return false;

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(vk->device, vk->atlas_image, &mem_reqs);

    uint32_t mem_type = find_memory_type(vk->physical_device,
        mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (mem_type == UINT32_MAX) return false;

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type,
    };

    if (vkAllocateMemory(vk->device, &alloc_info, NULL, &vk->atlas_memory) !=
        VK_SUCCESS)
        return false;

    vkBindImageMemory(vk->device, vk->atlas_image, vk->atlas_memory, 0);

    VkImageViewCreateInfo view_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = vk->atlas_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    if (vkCreateImageView(vk->device, &view_ci, NULL, &vk->atlas_view) !=
        VK_SUCCESS)
        return false;

    VkSamplerCreateInfo sampler_ci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxAnisotropy = 1.0f,
    };

    if (vkCreateSampler(vk->device, &sampler_ci, NULL, &vk->atlas_sampler) !=
        VK_SUCCESS)
        return false;

    VkBufferCreateInfo buf_ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = atlas_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (vkCreateBuffer(vk->device, &buf_ci, NULL, &vk->atlas_staging_buffer) !=
        VK_SUCCESS)
        return false;

    vkGetBufferMemoryRequirements(
        vk->device, vk->atlas_staging_buffer, &mem_reqs);
    mem_type = find_memory_type(vk->physical_device, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mem_type == UINT32_MAX) return false;

    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type;

    if (vkAllocateMemory(vk->device, &alloc_info, NULL,
            &vk->atlas_staging_memory) != VK_SUCCESS)
        return false;

    vkBindBufferMemory(
        vk->device, vk->atlas_staging_buffer, vk->atlas_staging_memory, 0);
    vkMapMemory(vk->device, vk->atlas_staging_memory, 0, atlas_size, 0,
        &vk->atlas_staging_mapped);

    VkDeviceSize vertex_buffer_size = sizeof(GlyphVertex) * 16384;
    VkBufferCreateInfo vbuf_ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vertex_buffer_size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    if (vkCreateBuffer(vk->device, &vbuf_ci, NULL, &vk->glyph_vertex_buffer) !=
        VK_SUCCESS)
        return false;

    vkGetBufferMemoryRequirements(
        vk->device, vk->glyph_vertex_buffer, &mem_reqs);
    mem_type = find_memory_type(vk->physical_device, mem_reqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mem_type == UINT32_MAX) return false;

    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = mem_type;

    if (vkAllocateMemory(vk->device, &alloc_info, NULL,
            &vk->glyph_vertex_memory) != VK_SUCCESS)
        return false;

    vkBindBufferMemory(
        vk->device, vk->glyph_vertex_buffer, vk->glyph_vertex_memory, 0);
    vkMapMemory(vk->device, vk->glyph_vertex_memory, 0, vertex_buffer_size, 0,
        &vk->glyph_vertex_mapped);

    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutCreateInfo dsl_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };

    if (vkCreateDescriptorSetLayout(
            vk->device, &dsl_ci, NULL, &vk->text_desc_layout) != VK_SUCCESS)
        return false;

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };

    VkDescriptorPoolCreateInfo pool_ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    if (vkCreateDescriptorPool(
            vk->device, &pool_ci, NULL, &vk->text_desc_pool) != VK_SUCCESS)
        return false;

    VkDescriptorSetAllocateInfo ds_ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vk->text_desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &vk->text_desc_layout,
    };

    if (vkAllocateDescriptorSets(vk->device, &ds_ai, &vk->text_desc_set) !=
        VK_SUCCESS)
        return false;

    VkDescriptorImageInfo desc_image_info = {
        .sampler = vk->atlas_sampler,
        .imageView = vk->atlas_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet write_ds = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = vk->text_desc_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &desc_image_info,
    };

    vkUpdateDescriptorSets(vk->device, 1, &write_ds, 0, NULL);

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(float) * 2,
    };

    VkPipelineLayoutCreateInfo pl_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &vk->text_desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pc_range,
    };

    if (vkCreatePipelineLayout(
            vk->device, &pl_ci, NULL, &vk->text_pipeline_layout) != VK_SUCCESS)
        return false;

    VkShaderModuleCreateInfo sm_ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(text_vert_spv),
        .pCode = text_vert_spv,
    };
    VkShaderModule vert_mod;
    if (vkCreateShaderModule(vk->device, &sm_ci, NULL, &vert_mod) != VK_SUCCESS)
        return false;

    sm_ci.codeSize = sizeof(text_frag_spv);
    sm_ci.pCode = text_frag_spv;
    VkShaderModule frag_mod;
    if (vkCreateShaderModule(vk->device, &sm_ci, NULL, &frag_mod) !=
        VK_SUCCESS) {
        vkDestroyShaderModule(vk->device, vert_mod, NULL);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_mod,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_mod,
            .pName = "main",
        }};

    VkVertexInputBindingDescription vibd = {
        .binding = 0,
        .stride = sizeof(GlyphVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription viad[3] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(GlyphVertex, pos),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(GlyphVertex, uv),
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(GlyphVertex, color),
        },
    };

    VkPipelineVertexInputStateCreateInfo pvis = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vibd,
        .vertexAttributeDescriptionCount = 3,
        .pVertexAttributeDescriptions = viad,
    };

    VkPipelineInputAssemblyStateCreateInfo pias = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineViewportStateCreateInfo pvs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo prs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo pmss = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState pcba = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo pcbs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &pcba,
    };

    VkDynamicState dyn_states[2] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo pds = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dyn_states,
    };

    VkPipelineRenderingCreateInfo rendering_ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &vk->swapchain_format,
    };

    VkGraphicsPipelineCreateInfo pipe_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_ci,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &pvis,
        .pInputAssemblyState = &pias,
        .pViewportState = &pvs,
        .pRasterizationState = &prs,
        .pMultisampleState = &pmss,
        .pColorBlendState = &pcbs,
        .pDynamicState = &pds,
        .layout = vk->text_pipeline_layout,
        .renderPass = VK_NULL_HANDLE,
    };

    VkResult res = vkCreateGraphicsPipelines(
        vk->device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk->text_pipeline);

    vkDestroyShaderModule(vk->device, vert_mod, NULL);
    vkDestroyShaderModule(vk->device, frag_mod, NULL);

    if (res != VK_SUCCESS) return false;

    // transition atlas image from UNDEFINED to SHADER_READ_ONLY_OPTIMAL
    VkCommandBuffer init_cmd;
    VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    if (vkAllocateCommandBuffers(vk->device, &cbai, &init_cmd) == VK_SUCCESS) {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(init_cmd, &begin_info);

        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk->atlas_image,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        };
        vkCmdPipelineBarrier(init_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
            &barrier);

        vkEndCommandBuffer(init_cmd);

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &init_cmd,
        };
        vkQueueSubmit(vk->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(vk->graphics_queue);

        vkFreeCommandBuffers(vk->device, vk->command_pool, 1, &init_cmd);
    }

    vk->atlas_initialized = true;
    return true;
}

static void
cleanup_text_pipeline(VulkanContext *vk)
{
    if (!vk || !vk->device) return;

    if (vk->text_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vk->device, vk->text_pipeline, NULL);
        vk->text_pipeline = VK_NULL_HANDLE;
    }
    if (vk->text_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk->device, vk->text_pipeline_layout, NULL);
        vk->text_pipeline_layout = VK_NULL_HANDLE;
    }
    if (vk->text_desc_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vk->device, vk->text_desc_pool, NULL);
        vk->text_desc_pool = VK_NULL_HANDLE;
    }
    if (vk->text_desc_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk->device, vk->text_desc_layout, NULL);
        vk->text_desc_layout = VK_NULL_HANDLE;
    }

    if (vk->glyph_vertex_mapped) {
        vkUnmapMemory(vk->device, vk->glyph_vertex_memory);
        vk->glyph_vertex_mapped = NULL;
    }
    if (vk->glyph_vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk->device, vk->glyph_vertex_buffer, NULL);
        vk->glyph_vertex_buffer = VK_NULL_HANDLE;
    }
    if (vk->glyph_vertex_memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk->device, vk->glyph_vertex_memory, NULL);
        vk->glyph_vertex_memory = VK_NULL_HANDLE;
    }

    if (vk->atlas_staging_mapped) {
        vkUnmapMemory(vk->device, vk->atlas_staging_memory);
        vk->atlas_staging_mapped = NULL;
    }
    if (vk->atlas_staging_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(vk->device, vk->atlas_staging_buffer, NULL);
        vk->atlas_staging_buffer = VK_NULL_HANDLE;
    }
    if (vk->atlas_staging_memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk->device, vk->atlas_staging_memory, NULL);
        vk->atlas_staging_memory = VK_NULL_HANDLE;
    }

    if (vk->atlas_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(vk->device, vk->atlas_sampler, NULL);
        vk->atlas_sampler = VK_NULL_HANDLE;
    }
    if (vk->atlas_view != VK_NULL_HANDLE) {
        vkDestroyImageView(vk->device, vk->atlas_view, NULL);
        vk->atlas_view = VK_NULL_HANDLE;
    }
    if (vk->atlas_image != VK_NULL_HANDLE) {
        vkDestroyImage(vk->device, vk->atlas_image, NULL);
        vk->atlas_image = VK_NULL_HANDLE;
    }
    if (vk->atlas_memory != VK_NULL_HANDLE) {
        vkFreeMemory(vk->device, vk->atlas_memory, NULL);
        vk->atlas_memory = VK_NULL_HANDLE;
    }

    vk->atlas_initialized = false;
}

int
vulkan_runtime_init(VulkanContext *vk, DisplayType display_type,
    void *native_display, void *native_window, uint32_t width, uint32_t height)
{
    // ------- create_instance()
    if (!vk) return 1;

    vk->initialized = false;
    vk->instance = VK_NULL_HANDLE;
    vk->surface = VK_NULL_HANDLE;

    VkApplicationInfo appInfo = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "zshell vulkan runtime",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "zshell render engine",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_3};

    const char *extensions[4];
    uint32_t ec = 0;

    extensions[ec++] = VK_KHR_SURFACE_EXTENSION_NAME;

#ifdef USE_WAYLAND
    if (display_type == DISPLAY_WAYLAND)
        extensions[ec++] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
#endif
#ifdef USE_X11
    if (display_type == DISPLAY_X11)
        extensions[ec++] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
#endif

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = ec,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = DEBUG ? 1 : 0,
        .ppEnabledLayerNames =
            DEBUG ? (const char *[]){"VK_LAYER_KHRONOS_validation"} : NULL,
    };

    VkResult result = vkCreateInstance(&createInfo, NULL, &vk->instance);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkCreateInstance failed: %d\n", result);
        goto fail;
    }

    result = VK_ERROR_INITIALIZATION_FAILED;

    // ---------create_surface()
#ifdef USE_X11
    if (display_type == DISPLAY_X11) {
        VkXcbSurfaceCreateInfoKHR surfaceInfo = {
            .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .connection = (xcb_connection_t *)native_display,
            .window = (xcb_window_t)(uintptr_t)native_window,
        };
        result = vkCreateXcbSurfaceKHR(
            vk->instance, &surfaceInfo, NULL, &vk->surface);
    }
#endif

#ifdef USE_WAYLAND
    if (display_type == DISPLAY_WAYLAND) {
        VkWaylandSurfaceCreateInfoKHR surfaceInfo = {
            .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .display = (struct wl_display *)native_display,
            .surface = (struct wl_surface *)native_window,
        };
        result = vkCreateWaylandSurfaceKHR(
            vk->instance, &surfaceInfo, NULL, &vk->surface);
    }
#endif

    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan surface: %d\n", result);
        goto fail;
    }

    // ----------pick_physical_devices()
    uint32_t count = 0;

    result = vkEnumeratePhysicalDevices(vk->instance, &count, NULL);
    if (result != VK_SUCCESS || count == 0) {
        fprintf(stderr, "Failed to enumerate physical devices: %d\n", result);
        goto fail;
    }

    VkPhysicalDevice *devices = malloc(sizeof(*devices) * count);
    if (!devices) goto fail;

    result = vkEnumeratePhysicalDevices(vk->instance, &count, devices);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to enumerate physical devices: %d\n", result);
        free(devices);
        goto fail;
    }

    for (uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties2 props = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        };

        vkGetPhysicalDeviceProperties2(devices[i], &props);

        uint32_t minor, major;
        fprintf(stderr, "GPU %u: %s (Vulkan %u.%u.%u)\n", i,
            props.properties.deviceName,
            major = VK_API_VERSION_MAJOR(props.properties.apiVersion),
            minor = VK_API_VERSION_MINOR(props.properties.apiVersion),
            VK_API_VERSION_PATCH(props.properties.apiVersion));
        if (major > 1 || (major == 1 && minor >= 3)) {
            vk->physical_device = devices[i];
            break;
        }
    }
    free(devices);

    // ----------create_logical_device_and_queue()
    vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device, &count, NULL);

    VkQueueFamilyProperties *queue_families =
        malloc(count * sizeof(VkQueueFamilyProperties));

    vkGetPhysicalDeviceQueueFamilyProperties(
        vk->physical_device, &count, queue_families);

    vk->graphics_family = UINT32_MAX;
    vk->present_family = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        VkBool32 present_support = VK_FALSE;

        result = vkGetPhysicalDeviceSurfaceSupportKHR(
            vk->physical_device, i, vk->surface, &present_support);

        if (result != VK_SUCCESS) {
            fprintf(stderr,
                "Failed to query surface support for queue family %u: %d\n", i,
                result);
            free(queue_families);
            goto fail;
        }

        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            present_support) { // FIXME: case of two separate queues
            vk->graphics_family = i;
            vk->present_family = i;
            break;
        }
    }
    free(queue_families);
    if (vk->graphics_family == UINT32_MAX) {
        fprintf(stderr, "Failed to find a queue family\n");
        goto fail;
    }
    VkBool32 phy_surf_supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(vk->physical_device,
        vk->graphics_family, vk->surface, &phy_surf_supported);
    if (phy_surf_supported != VK_TRUE) {
        fprintf(stderr, "Surface unsupported!\n");
        goto fail;
    }

    // FIXME: graphics and present queues
    VkDeviceQueueCreateInfo dqi = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vk->graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &(float){0.5f},
    };

    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .dynamicRendering = VK_TRUE,
    };

    VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features13,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &dqi,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = (const char *[]){"VK_KHR_swapchain"},
    };

    result = vkCreateDevice(vk->physical_device, &dci, NULL, &vk->device);

    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device: %d\n", result);
        goto fail;
    }
    vkGetDeviceQueue(vk->device, vk->graphics_family, 0, &vk->graphics_queue);
    vk->present_queue = vk->graphics_queue;
    // create_swapchain()
    VkSurfaceCapabilitiesKHR surf_caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        vk->physical_device, vk->surface, &surf_caps);
    uint32_t surf_form_count = 0;

    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        vk->physical_device, vk->surface, &surf_form_count, NULL);

    if (result != VK_SUCCESS || surf_form_count == 0) goto fail;

    VkSurfaceFormatKHR *surf_forms =
        malloc(surf_form_count * sizeof(*surf_forms));

    if (!surf_forms) goto fail;

    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        vk->physical_device, vk->surface, &surf_form_count, surf_forms);

    if (result != VK_SUCCESS) {
        free(surf_forms);
        goto fail;
    }
    uint32_t pres_mode_count = 0;

    result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        vk->physical_device, vk->surface, &pres_mode_count, NULL);

    if (result != VK_SUCCESS || pres_mode_count == 0) goto fail;

    VkPresentModeKHR *pres_modes =
        malloc(pres_mode_count * sizeof(*pres_modes));

    if (!pres_modes) goto fail;

    result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        vk->physical_device, vk->surface, &pres_mode_count, pres_modes);

    if (result != VK_SUCCESS) {
        free(pres_modes);
        goto fail;
    }
    VkSurfaceFormatKHR selected_format;
    for (uint32_t i = 0; i < surf_form_count; ++i) {
        if (surf_forms[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            surf_forms[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selected_format = surf_forms[i];
            break;
        }
    }
    free(surf_forms);
    vk->swapchain_format = selected_format.format;
    vk->swapchain_color_space = selected_format.colorSpace;

    if (selected_format.format == VK_FORMAT_UNDEFINED) {
        fprintf(stderr, "Format not supported\n");
        goto fail;
    }
    VkPresentModeKHR selected_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < pres_mode_count; i++) {
        if (pres_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            selected_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    free(pres_modes);

    if (surf_caps.currentExtent.width != UINT32_MAX)
        vk->extent = surf_caps.currentExtent;
    else
        vk->extent = (VkExtent2D){
            .width = clamp(width, surf_caps.minImageExtent.width,
                surf_caps.maxImageExtent.width),
            .height = clamp(height, surf_caps.minImageExtent.height,
                surf_caps.maxImageExtent.height),
        };

    uint32_t image_count = surf_caps.minImageCount + 1;
    if (surf_caps.maxImageCount > 0 && image_count > surf_caps.maxImageCount) {
        image_count = surf_caps.maxImageCount;
    }
    vk->image_count = image_count;

    VkCompositeAlphaFlagBitsKHR composite_alpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR alpha_flags[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (size_t i = 0; i < sizeof(alpha_flags) / sizeof(alpha_flags[0]); i++) {
        if (surf_caps.supportedCompositeAlpha & alpha_flags[i]) {
            composite_alpha = alpha_flags[i];
            break;
        }
    }

    VkSwapchainCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk->surface,
        .minImageCount = vk->image_count,
        .imageFormat = vk->swapchain_format,
        .imageColorSpace = vk->swapchain_color_space,
        .imageExtent = vk->extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surf_caps.currentTransform,
        .compositeAlpha = composite_alpha,
        // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        .presentMode = selected_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    result = vkCreateSwapchainKHR(vk->device, &sci, NULL, &vk->swapchain);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkCreateSwapchainKHR failed: %d\n", result);
        goto fail;
    }

    result = vkGetSwapchainImagesKHR(
        vk->device, vk->swapchain, &vk->image_count, NULL);
    if (result != VK_SUCCESS || vk->image_count == 0) {
        fprintf(stderr, "vkGetSwapchainImagesKHR count failed: %d\n", result);
        goto fail;
    }

    vk->images = malloc(sizeof(VkImage) * vk->image_count);
    if (!vk->images) goto fail;

    result = vkGetSwapchainImagesKHR(
        vk->device, vk->swapchain, &vk->image_count, vk->images);
    if (result != VK_SUCCESS) {
        fprintf(
            stderr, "vkGetSwapchainImagesKHR retrieval failed: %d\n", result);
        goto fail;
    }

    vk->image_views = malloc(sizeof(VkImageView) * vk->image_count);
    if (!vk->image_views) goto fail;

    for (uint32_t i = 0; i < vk->image_count; i++) {
        vk->image_views[i] = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < vk->image_count; i++) {
        VkImageViewCreateInfo ivci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = vk->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = vk->swapchain_format,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        result =
            vkCreateImageView(vk->device, &ivci, NULL, &vk->image_views[i]);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "vkCreateImageView %u failed: %d\n", i, result);
            goto fail;
        }
    }

    VkCommandPoolCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk->graphics_family,
    };
    result = vkCreateCommandPool(vk->device, &cpci, NULL, &vk->command_pool);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkCreateCommandPool failed: %d\n", result);
        goto fail;
    }

    vk->command_buffers = malloc(sizeof(VkCommandBuffer) * vk->image_count);
    if (!vk->command_buffers) goto fail;

    VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = vk->image_count,
    };
    result = vkAllocateCommandBuffers(vk->device, &cbai, vk->command_buffers);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkAllocateCommandBuffers failed: %d\n", result);
        goto fail;
    }

    // sync objects
    vk->image_available_sems = malloc(sizeof(VkSemaphore) * vk->image_count);
    vk->render_finished_sems = malloc(sizeof(VkSemaphore) * vk->image_count);
    vk->in_flight_fences = malloc(sizeof(VkFence) * vk->image_count);
    if (!vk->image_available_sems || !vk->render_finished_sems ||
        !vk->in_flight_fences) {
        goto fail;
    }

    for (uint32_t i = 0; i < vk->image_count; i++) {
        vk->image_available_sems[i] = VK_NULL_HANDLE;
        vk->render_finished_sems[i] = VK_NULL_HANDLE;
        vk->in_flight_fences[i] = VK_NULL_HANDLE;
    }

    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (uint32_t i = 0; i < vk->image_count; i++) {
        if (vkCreateSemaphore(vk->device, &sem_info, NULL,
                &vk->image_available_sems[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vk->device, &sem_info, NULL,
                &vk->render_finished_sems[i]) != VK_SUCCESS ||
            vkCreateFence(vk->device, &fence_info, NULL,
                &vk->in_flight_fences[i]) != VK_SUCCESS) {
            fprintf(
                stderr, "Failed to create Vulkan synchronization objects\n");
            goto fail;
        }
    }
    vk->current_frame = 0;

    if (!init_text_pipeline(vk)) {
        fprintf(stderr, "Failed to initialize Vulkan text pipeline\n");
        goto fail;
    }

    vk->initialized = true;
    return 0;

fail:
    vulkan_runtime_cleanup(vk);
    return 1;
}

int
vulkan_runtime_cleanup(VulkanContext *vk)
{
    if (!vk) return 0;

    if (vk->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(vk->device);

        if (vk->image_available_sems) {
            for (uint32_t i = 0; i < vk->image_count; i++) {
                if (vk->image_available_sems[i] != VK_NULL_HANDLE) {
                    vkDestroySemaphore(
                        vk->device, vk->image_available_sems[i], NULL);
                }
            }
            free(vk->image_available_sems);
            vk->image_available_sems = NULL;
        }
        if (vk->render_finished_sems) {
            for (uint32_t i = 0; i < vk->image_count; i++) {
                if (vk->render_finished_sems[i] != VK_NULL_HANDLE) {
                    vkDestroySemaphore(
                        vk->device, vk->render_finished_sems[i], NULL);
                }
            }
            free(vk->render_finished_sems);
            vk->render_finished_sems = NULL;
        }
        if (vk->in_flight_fences) {
            for (uint32_t i = 0; i < vk->image_count; i++) {
                if (vk->in_flight_fences[i] != VK_NULL_HANDLE) {
                    vkDestroyFence(vk->device, vk->in_flight_fences[i], NULL);
                }
            }
            free(vk->in_flight_fences);
            vk->in_flight_fences = NULL;
        }

        if (vk->command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(vk->device, vk->command_pool, NULL);
            vk->command_pool = VK_NULL_HANDLE;
        }

        if (vk->command_buffers) {
            free(vk->command_buffers);
            vk->command_buffers = NULL;
        }

        if (vk->image_views) {
            for (uint32_t i = 0; i < vk->image_count; i++) {
                if (vk->image_views[i] != VK_NULL_HANDLE) {
                    vkDestroyImageView(vk->device, vk->image_views[i], NULL);
                }
            }
            free(vk->image_views);
            vk->image_views = NULL;
        }

        if (vk->images) {
            free(vk->images);
            vk->images = NULL;
        }

        if (vk->swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(vk->device, vk->swapchain, NULL);
            vk->swapchain = VK_NULL_HANDLE;
        }

        cleanup_text_pipeline(vk);

        vkDestroyDevice(vk->device, NULL);
        vk->device = VK_NULL_HANDLE;
    }

    if (vk->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(vk->instance, vk->surface, NULL);
        vk->surface = VK_NULL_HANDLE;
    }

    if (vk->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(vk->instance, NULL);
        vk->instance = VK_NULL_HANDLE;
    }

    vk->initialized = false;
    return 0;
}

int
vulkan_runtime_render_frame(
    VulkanContext *vk, const RenderList *render_list, GlyphAtlas *atlas)
{
    if (!vk || !vk->initialized) return 1;

    uint32_t frame = vk->current_frame;

    // wait for preceding frame
    vkWaitForFences(
        vk->device, 1, &vk->in_flight_fences[frame], VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult result =
        vkAcquireNextImageKHR(vk->device, vk->swapchain, UINT64_MAX,
            vk->image_available_sems[frame], VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return 0;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "vkAcquireNextImageKHR failed: %d\n", result);
        return 1;
    }

    vkResetFences(vk->device, 1, &vk->in_flight_fences[frame]);

    VkCommandBuffer cmd = vk->command_buffers[frame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo cbbi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &cbbi);

    // atlas texture update if dirty
    if (atlas && glyph_atlas_is_dirty(atlas) && vk->atlas_staging_mapped) {
        memcpy(vk->atlas_staging_mapped, atlas->buffer,
            atlas->width * atlas->height);

        VkImageMemoryBarrier barrier_to_dst = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk->atlas_image,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
            &barrier_to_dst);

        VkBufferImageCopy copy_region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageOffset = {0, 0, 0},
            .imageExtent = {atlas->width, atlas->height, 1},
        };
        vkCmdCopyBufferToImage(cmd, vk->atlas_staging_buffer, vk->atlas_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

        VkImageMemoryBarrier barrier_to_shader = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk->atlas_image,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
            &barrier_to_shader);

        glyph_atlas_clear_dirty(atlas);
    }

    // barrier: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier barrier_to_color = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vk->images[image_index],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
        &barrier_to_color);

    VkClearValue bg_color = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
    VkRenderingAttachmentInfo color_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = vk->image_views[image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = bg_color,
    };

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea =
            {
                .offset = {0, 0},
                .extent = vk->extent,
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
    };

    vkCmdBeginRendering(cmd, &rendering_info);

    // draw commands
    if (render_list && render_list->commands) {
        for (size_t i = 0; i < render_list->count; i++) {
            const RenderCommand *rc = &render_list->commands[i];
            if (rc->type == CMD_RECT) {
                const RectCommand *rect = &rc->rect;
                if (rect->width <= 0 || rect->height <= 0) continue;

                int32_t rx = (int32_t)rect->x;
                int32_t ry = (int32_t)rect->y;
                uint32_t rw = (uint32_t)rect->width;
                uint32_t rh = (uint32_t)rect->height;

                float r = ((rect->color >> 24) & 0xFF) / 255.0f;
                float g = ((rect->color >> 16) & 0xFF) / 255.0f;
                float b = ((rect->color >> 8) & 0xFF) / 255.0f;
                float a = (rect->color & 0xFF) / 255.0f;

                VkClearAttachment clear_rect_attachment = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .colorAttachment = 0,
                    .clearValue = {{{r, g, b, a}}},
                };

                VkClearRect clear_rect = {
                    .rect =
                        {
                            .offset = {rx, ry},
                            .extent = {rw, rh},
                        },
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                };

                vkCmdClearAttachments(
                    cmd, 1, &clear_rect_attachment, 1, &clear_rect);
            }
        }

        if (vk->glyph_vertex_mapped && vk->text_pipeline) {
            size_t glyph_count = 0;
            GlyphVertex *vptr = (GlyphVertex *)vk->glyph_vertex_mapped;
            size_t max_glyphs = 16384 / 6;

            for (size_t i = 0;
                i < render_list->count && glyph_count < max_glyphs; i++) {
                const RenderCommand *rc = &render_list->commands[i];
                if (rc->type == CMD_GLYPH) {
                    const GlyphCommand *g = &rc->glyph;
                    if (g->width <= 0 || g->height <= 0) continue;

                    float r = ((g->color >> 24) & 0xFF) / 255.0f;
                    float gr = ((g->color >> 16) & 0xFF) / 255.0f;
                    float b = ((g->color >> 8) & 0xFF) / 255.0f;
                    float a = (g->color & 0xFF) / 255.0f;

                    float x0 = g->x;
                    float y0 = g->y;
                    float x1 = g->x + g->width;
                    float y1 = g->y + g->height;

                    float u0 = g->u0;
                    float v0 = g->v0;
                    float u1 = g->u1;
                    float v1 = g->v1;

                    vptr[0] = (GlyphVertex){{x0, y0}, {u0, v0}, {r, gr, b, a}};
                    vptr[1] = (GlyphVertex){{x1, y0}, {u1, v0}, {r, gr, b, a}};
                    vptr[2] = (GlyphVertex){{x0, y1}, {u0, v1}, {r, gr, b, a}};

                    vptr[3] = (GlyphVertex){{x1, y0}, {u1, v0}, {r, gr, b, a}};
                    vptr[4] = (GlyphVertex){{x1, y1}, {u1, v1}, {r, gr, b, a}};
                    vptr[5] = (GlyphVertex){{x0, y1}, {u0, v1}, {r, gr, b, a}};

                    vptr += 6;
                    glyph_count++;
                }
            }

            if (glyph_count > 0) {
                VkViewport viewport = {
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = (float)vk->extent.width,
                    .height = (float)vk->extent.height,
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f,
                };
                vkCmdSetViewport(cmd, 0, 1, &viewport);

                VkRect2D scissor = {
                    .offset = {0, 0},
                    .extent = vk->extent,
                };
                vkCmdSetScissor(cmd, 0, 1, &scissor);

                vkCmdBindPipeline(
                    cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->text_pipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    vk->text_pipeline_layout, 0, 1, &vk->text_desc_set, 0,
                    NULL);

                float screen_size[2] = {
                    (float)vk->extent.width, (float)vk->extent.height};
                vkCmdPushConstants(cmd, vk->text_pipeline_layout,
                    VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 2,
                    screen_size);

                VkDeviceSize voffset = 0;
                vkCmdBindVertexBuffers(
                    cmd, 0, 1, &vk->glyph_vertex_buffer, &voffset);
                vkCmdDraw(cmd, (uint32_t)(glyph_count * 6), 1, 0, 0);
            }
        }
    }

    vkCmdEndRendering(cmd);

    // barrier: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
    VkImageMemoryBarrier barrier_to_present = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vk->images[image_index],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = 0,
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
        &barrier_to_present);

    vkEndCommandBuffer(cmd);

    // queue submit
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk->image_available_sems[frame],
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &vk->render_finished_sems[image_index],
    };

    result = vkQueueSubmit(
        vk->graphics_queue, 1, &submit_info, vk->in_flight_fences[frame]);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkQueueSubmit failed: %d\n", result);
        return 1;
    }

    // queue present
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk->render_finished_sems[image_index],
        .swapchainCount = 1,
        .pSwapchains = &vk->swapchain,
        .pImageIndices = &image_index,
    };

    result = vkQueuePresentKHR(vk->present_queue, &present_info);

    vk->current_frame = (vk->current_frame + 1) % vk->image_count;

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return 0;
    } else if (result != VK_SUCCESS) {
        fprintf(stderr, "vkQueuePresentKHR failed: %d\n", result);
        return 1;
    }

    return 0;
}
