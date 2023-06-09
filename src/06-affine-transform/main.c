#include "../common/vulkan-tutorial.h"

// A struct for vertex input data.
typedef struct Vertex_t {
    float pos[3];
} Vertex;

// A struct for organizing the layout of push constant data.
// NOTE: プッシュコンスタントの構造。
// NOTE: のちのち変更されるため、vulkan-tutorial.hではなくここで定義しておく。
typedef struct PushConstant_t {
    float scl[4];
    float rot[4];
    float trs[4];
} PushConstant;

int main() {
    // window
    GLFWwindow* window;
    {
        const int res = glfwInit();
        CHECK(res == GLFW_TRUE, "failed to init GLFW.");
        SET_GLFW_ERROR_CALLBACK();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
        CHECK(window != NULL, "failed to create a window.");
    }

    // instance
    VkInstance instance;
    {
        const VkApplicationInfo ai = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO,
            NULL,
            "VulkanApplication\0",
            0,
            "VulkanApplication\0",
            VK_MAKE_VERSION(1, 0, 0),
            VK_API_VERSION_1_2,
        };
        const char *layer_names[] = INST_LAYER_NAMES;
        const char *ext_names[] = INST_EXT_NAMES;
        const VkInstanceCreateInfo ci = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            NULL,
            0,
            &ai,
            INST_LAYER_NAMES_CNT,
            layer_names,
            INST_EXT_NAMES_CNT,
            ext_names,
        };
        CHECK_VK(vkCreateInstance(&ci, NULL, &instance), "failed to create a Vulkan instance.");
    }

    // debug
    SET_VULKAN_DEBUG_CALLBACK(instance);

    // physical device
    VkPhysicalDevice phys_device;
    VkPhysicalDeviceMemoryProperties phys_device_memory_prop;
    {
        uint32_t cnt = 0;
        CHECK_VK(vkEnumeratePhysicalDevices(instance, &cnt, NULL), "failed to get the number of physical devices.");
        VkPhysicalDevice *phys_devices = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * cnt);
        CHECK_VK(vkEnumeratePhysicalDevices(instance, &cnt, phys_devices), "failed to enumerate physical devices.");
        phys_device = phys_devices[0];
        vkGetPhysicalDeviceMemoryProperties(phys_device, &phys_device_memory_prop);
        free(phys_devices);
    }

    // queue family index
    int32_t queue_family_index = -1;
    {
        uint32_t cnt = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &cnt, NULL);
        VkQueueFamilyProperties *props = (VkQueueFamilyProperties *)malloc(sizeof(VkQueueFamilyProperties) * cnt);
        vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &cnt, props);
        for (int32_t i = 0; i < cnt; ++i) {
            if ((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0) {
                queue_family_index = i;
                break;
            }
        }
        CHECK(queue_family_index >= 0, "failed to find a queue family index.");
        free(props);
    }

    // device
    VkDevice device;
    {
        const float queue_priorities[] = { 1.0 };
        const VkDeviceQueueCreateInfo queue_cis[] = {
            {
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                NULL,
                0,
                queue_family_index,
                1,
                queue_priorities,
            },
        };
        const char *ext_names[] = DEVICE_EXT_NAMES;
        const VkDeviceCreateInfo ci = {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            NULL,
            0,
            1,
            queue_cis,
            0,
            NULL,
            DEVICE_EXT_NAMES_CNT,
            ext_names,
            NULL,
        };
        CHECK_VK(vkCreateDevice(phys_device, &ci, NULL, &device), "failed to create a device.");
    }

    // queue
    VkQueue queue;
    vkGetDeviceQueue(device, queue_family_index, 0, &queue);

    // command pool
    VkCommandPool command_pool;
    {
        const VkCommandPoolCreateInfo ci = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            NULL,
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            queue_family_index,
        };
        CHECK_VK(vkCreateCommandPool(device, &ci, NULL, &command_pool), "failed to create a command pool.");
    }

    // command buffer
    VkCommandBuffer command_buffer;
    {
        const VkCommandBufferAllocateInfo ai = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            NULL,
            command_pool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1,
        };
        CHECK_VK(vkAllocateCommandBuffers(device, &ai, &command_buffer), "failed to allocate a command buffer.");
    }

    // fence
    VkFence fence;
    {
        const VkFenceCreateInfo ci = {
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            NULL,
            VK_FENCE_CREATE_SIGNALED_BIT,
        };
        CHECK_VK(vkCreateFence(device, &ci, NULL, &fence), "failed to create a fence.");
    }

    // semaphores
    VkSemaphore wait_semaphore, signal_semaphore;
    {
        const VkSemaphoreCreateInfo ci = {
            VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            NULL,
            0,
        };
        CHECK_VK(vkCreateSemaphore(device, &ci, NULL, &wait_semaphore), "failed to create a wait semaphore.");
        CHECK_VK(vkCreateSemaphore(device, &ci, NULL, &signal_semaphore), "failed to create a signal semaphore.");
    }

    // surface
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_format;
    VkSurfaceCapabilitiesKHR surface_capabilities;
    {
        CHECK_VK(glfwCreateWindowSurface(instance, window, NULL, &surface), "failed to create a surface.");
        uint32_t cnt = 0;
        CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &cnt, NULL), "failed to get the number of surface formats.");
        VkSurfaceFormatKHR *formats = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * cnt);
        CHECK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &cnt, formats), "failed to get surface formats.");
        int32_t index = -1;
        for (int32_t i = 0; i < cnt; ++i) {
            if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
                index = i;
                break;
            }
        }
        CHECK(index >= 0, "failed to get a surface format index.");
        surface_format = formats[index];
        CHECK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &surface_capabilities), "failed to get a surface capabilities.");
        free(formats);
    }

    // swapchain
    VkSwapchainKHR swapchain;
    {
        const uint32_t min_image_count = surface_capabilities.minImageCount > 2 ? surface_capabilities.minImageCount : 2;
        const VkSwapchainCreateInfoKHR ci = {
            VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            NULL,
            0,
            surface,
            min_image_count,
            surface_format.format,
            surface_format.colorSpace,
            surface_capabilities.currentExtent,
            1,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            NULL,
            surface_capabilities.currentTransform,
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_PRESENT_MODE_FIFO_KHR,
            VK_TRUE,
            VK_NULL_HANDLE,
        };
        CHECK_VK(vkCreateSwapchainKHR(device, &ci, NULL, &swapchain), "failed to create a swapchain.");
    }

    // image views
    uint32_t image_views_cnt;
    VkImageView *image_views;
    {
        uint32_t images_cnt = 0;
        CHECK_VK(vkGetSwapchainImagesKHR(device, swapchain, &images_cnt, NULL), "failed to get the number of swapchain images.");
        VkImage *images = (VkImage *)malloc(sizeof(VkImage) * images_cnt);
        CHECK_VK(vkGetSwapchainImagesKHR(device, swapchain, &images_cnt, images), "failed to get swapchain images.");
        image_views_cnt = images_cnt;
        image_views = (VkImageView *)malloc(sizeof(VkImageView) * image_views_cnt);
        VkImageViewCreateInfo ci = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            NULL,
            0,
            NULL,
            VK_IMAGE_VIEW_TYPE_2D,
            surface_format.format,
            {
                VK_COMPONENT_SWIZZLE_R,
                VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B,
                VK_COMPONENT_SWIZZLE_A,
            },
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        for (int32_t i = 0; i < images_cnt; ++i) {
            ci.image = images[i];
            CHECK_VK(vkCreateImageView(device, &ci, NULL, &image_views[i]), "failed to create an image view.");
        }
        free(images);
    }

    // render pass
    VkRenderPass render_pass;
    {
        const VkAttachmentDescription attachment_descs[] = {
            {
                0,
                surface_format.format,
                VK_SAMPLE_COUNT_1_BIT,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
        };
        const VkAttachmentReference attachment_refs[] = {
            {
                0,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        };
        const VkSubpassDescription subpass_descs[] = {
            {
                0,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                0,
                NULL,
                1,
                attachment_refs,
                NULL,
                NULL,
                0,
                NULL,
            },
        };
        const VkRenderPassCreateInfo ci = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            NULL,
            0,
            1,
            attachment_descs,
            1,
            subpass_descs,
            0,
            NULL,
        };
        CHECK_VK(vkCreateRenderPass(device, &ci, NULL, &render_pass), "failed to create a render pass.");
    }

    // framebuffers
    VkFramebuffer *framebuffers;
    {
        VkFramebufferCreateInfo ci = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            NULL,
            0,
            render_pass,
            1,
            NULL,
            surface_capabilities.currentExtent.width,
            surface_capabilities.currentExtent.height,
            1,
        };
        framebuffers = (VkFramebuffer *)malloc(sizeof(VkFramebuffer) * image_views_cnt);
        for (int32_t i = 0; i < image_views_cnt; ++i) {
            ci.pAttachments = &image_views[i];
            CHECK_VK(vkCreateFramebuffer(device, &ci, NULL, &framebuffers[i]), "failed to create a framebuffer.");
        }
    }

    // shaders
    VkShaderModule vert_shader;
    VkShaderModule frag_shader;
    {
        // vertex shader
        int bin_vert_size;
        char *bin_vert = read_bin("./shader.vert.spv", &bin_vert_size);
        CHECK(bin_vert != NULL, "failed to read shader.vert.spv.");
        const VkShaderModuleCreateInfo vert_ci = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            NULL,
            0,
            bin_vert_size,
            (const uint32_t*)bin_vert,
        };
        CHECK_VK(vkCreateShaderModule(device, &vert_ci, NULL, &vert_shader), "failed to create a vertex shader module.");
        // fragment shader
        int bin_frag_size;
        char *bin_frag = read_bin("./shader.frag.spv", &bin_frag_size);
        CHECK(bin_frag != NULL, "failed to read shader.frag.spv.");
        const VkShaderModuleCreateInfo frag_ci = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            NULL,
            0,
            bin_frag_size,
            (const uint32_t*)bin_frag,
        };
        CHECK_VK(vkCreateShaderModule(device, &frag_ci, NULL, &frag_shader), "failed to create a fragment shader module.");
        free(bin_vert);
        free(bin_frag);
    }

    // pipeline
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    {
        const VkPushConstantRange push_constant_ranges[] = {
            {
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(PushConstant),
            },
        };
        const VkPipelineLayoutCreateInfo pipeline_layout_ci = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            NULL,
            0,
            0,
            NULL,
            1,
            push_constant_ranges,
        };
        CHECK_VK(vkCreatePipelineLayout(device, &pipeline_layout_ci, NULL, &pipeline_layout), "failed to create a pipeline layout.");

        // shaders
        const VkPipelineShaderStageCreateInfo shader_cis[2] = {
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                NULL,
                0,
                VK_SHADER_STAGE_VERTEX_BIT,
                vert_shader,
                "main",
                NULL,
            },
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                NULL,
                0,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                frag_shader,
                "main",
                NULL,
            },
        };

        // vertex input
        const VkVertexInputBindingDescription vert_inp_binding_dcs[] = {
            { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX },
        };
        const VkVertexInputAttributeDescription vert_inp_attr_dcs[] = {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
        };
        const VkPipelineVertexInputStateCreateInfo vert_inp_ci = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            NULL,
            0,
            1,
            vert_inp_binding_dcs,
            1,
            vert_inp_attr_dcs,
        };

        // input assembly
        const VkPipelineInputAssemblyStateCreateInfo inp_as_ci = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            NULL,
            0,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_FALSE,
        };

        // viewport
        const VkViewport viewports[] = {
            {
                0.0f,
                0.0f,
                surface_capabilities.currentExtent.width,
                surface_capabilities.currentExtent.height,
                0.0f,
                1.0f,
            },
        };
        const VkRect2D scissors[] = {
            { {0, 0}, surface_capabilities.currentExtent },
        };
        const VkPipelineViewportStateCreateInfo viewport_ci = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            NULL,
            0,
            1,
            viewports,
            1,
            scissors,
        };

        // rasterization
        const VkPipelineRasterizationStateCreateInfo raster_ci = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            NULL,
            0,
            VK_FALSE,
            VK_FALSE,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_BACK_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE,
            VK_FALSE,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
        };

        // multisample
        const VkPipelineMultisampleStateCreateInfo multisample_ci = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            NULL,
            0,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FALSE,
            0.0f,
            NULL,
            VK_FALSE,
            VK_FALSE,
        };

        // color blend
        const VkPipelineColorBlendAttachmentState color_blend_states[] = {
            {
                VK_TRUE,
                VK_BLEND_FACTOR_SRC_ALPHA,
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                VK_BLEND_OP_ADD,
                VK_BLEND_FACTOR_SRC_ALPHA,
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                VK_BLEND_OP_ADD,
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            }
        };
        const VkPipelineColorBlendStateCreateInfo color_blend_ci = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            NULL,
            0,
            VK_FALSE,
            (VkLogicOp)0,
            1,
            color_blend_states,
            {0.0f, 0.0f, 0.0f, 0.0f},
        };

        // pipeline
        const VkGraphicsPipelineCreateInfo cis[] = {
            {
                VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                NULL,
                0,
                2,
                shader_cis,
                &vert_inp_ci,
                &inp_as_ci,
                NULL,
                &viewport_ci,
                &raster_ci,
                &multisample_ci,
                NULL,
                &color_blend_ci,
                NULL,
                pipeline_layout,
                render_pass,
                0,
                NULL,
                0,
            },
        };
        CHECK_VK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, cis, NULL, &pipeline), "failed to create a pipeline.");
    }

    // models
    Model models[2];
    // model 0
    {
        // NOTE: 正三角形。
        const Vertex vtxs[3] = {
            { { -0.5f,   0.25f, 0.0f } },
            { {  0.5f,   0.25f, 0.0f } },
            { {  0.0f, -0.616f, 0.0f } },
        };
        const uint32_t idxs[3] = { 0, 1, 2 };
        CHECK_VK(
            create_model(
                device,
                &phys_device_memory_prop,
                3,
                sizeof(Vertex) * 3,
                (const float *)vtxs,
                (const uint32_t *)idxs,
                &models[0]
            ),
            "failed to create the first model."
        );
    }
    // model 1
    {
        // NOTE: 正方形。
        const Vertex vtxs[4] = {
            { { -0.5f,  0.5f, 0.0f } }, // NOTE: 左下。
            { {  0.5f,  0.5f, 0.0f } }, // NOTE: 右下。
            { {  0.5f, -0.5f, 0.0f } }, // NOTE: 右上。
            { { -0.5f, -0.5f, 0.0f } }, // NOTE: 左上。
        };
        const uint32_t idxs[6] = { 0, 1, 2, 0, 2, 3 };
        CHECK_VK(
            create_model(
                device,
                &phys_device_memory_prop,
                6,
                sizeof(Vertex) * 4,
                (const float *)vtxs,
                (const uint32_t *)idxs,
                &models[1]
            ),
            "failed to create the second model."
        );
    }

    // NOTE: 二つのモデル(正確には二回の描画)のためのプッシュコンスタントをここで定義しておく。
    PushConstant push_constants[2] = {
        {
            { 0.5, 0.5, 1.0, 0.0 }, // NOTE: x,y方向に0.5倍
            { 0.0, 0.0, 0.0, 0.0 },
            { -0.5, -0.5, 0.5, 0.0 }, // NOTE: x,y方向に-0.5、z方向に0.5移動
        },
        {
            { 0.5, 0.5, 1.0, 0.0 }, // NOTE: x,y方向に0.5倍
            { 0.0, 0.0, 0.0, 0.0 },
            { 0.25, 0.25, 0.5, 0.0 }, // NOTE: x,y方向に0.25、z方向に0.5移動
        },
    };

    // mainloop
    while (1) {
        if (glfwWindowShouldClose(window))
            break;
        glfwPollEvents();

        // NOTE: 毎フレームZ軸回りに0.01ラジアン回転する。
        push_constants[0].rot[2] += 0.01f;
        // NOTE: 毎フレームX軸Y軸回りに0.01ラジアン回転する。
        push_constants[1].rot[0] += 0.01f;
        push_constants[1].rot[1] += 0.01f;

        // prepare
        int img_idx;
        WARN_VK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, wait_semaphore, VK_NULL_HANDLE, &img_idx), "failed to acquire a next image index.");
        WARN_VK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "failed to wait for a fence.");
        WARN_VK(vkResetFences(device, 1, &fence), "failed to reset a fence.");
        WARN_VK(vkResetCommandBuffer(command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT), "failed to reset a command buffer.");

        // begin
        const VkCommandBufferBeginInfo cmd_bi = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            NULL,
            0,
            NULL,
        };
        WARN_VK(vkBeginCommandBuffer(command_buffer, &cmd_bi), "failed to begin to record commands to render.");
        const VkClearValue clear_values[] = {
            {{ SCREEN_CLEAR_RGBA }},
        };
        const VkRenderPassBeginInfo rp_bi = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            NULL,
            render_pass,
            framebuffers[img_idx],
            { {0, 0}, surface_capabilities.currentExtent },
            1,
            clear_values,
        };
        vkCmdBeginRenderPass(command_buffer, &rp_bi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // draw
        // NOTE: 二つのモデルを描画する。
        for (int i = 0; i < 2; ++i) {
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(command_buffer, 0, 1, &models[i].vertex.buffer, &offset);
            vkCmdBindIndexBuffer(command_buffer, models[i].index.buffer, offset, VK_INDEX_TYPE_UINT32);
            vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), (const void *)&push_constants[i]);
            vkCmdDrawIndexed(command_buffer, models[i].index_cnt, 1, 0, 0, 0);
        }

        // end
        vkCmdEndRenderPass(command_buffer);
        vkEndCommandBuffer(command_buffer);
        const VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const VkSubmitInfo submit_info = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            NULL,
            1,
            &wait_semaphore,
            &wait_stage_mask,
            1,
            &command_buffer,
            1,
            &signal_semaphore,
        };
        WARN_VK(vkQueueSubmit(queue, 1, &submit_info, fence), "failed to submit commands to queue.");
        VkResult res;
        const VkPresentInfoKHR pi = {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            NULL,
            1,
            &signal_semaphore,
            1,
            &swapchain,
            &img_idx,
            &res,
        };
        WARN_VK(vkQueuePresentKHR(queue, &pi), "failed to enqueue present command.");
        WARN_VK(res, "failed to present.");
    }

    // termination
    vkDeviceWaitIdle(device);
    for (int i = 0; i < 2; ++i) {
        vkFreeMemory(device, models[i].vertex.memory, NULL);
        vkFreeMemory(device, models[i].index.memory, NULL);
        vkDestroyBuffer(device, models[i].vertex.buffer, NULL);
        vkDestroyBuffer(device, models[i].index.buffer, NULL);
    }
    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    vkDestroyShaderModule(device, frag_shader, NULL);
    vkDestroyShaderModule(device, vert_shader, NULL);
    for (uint32_t i = 0; i < image_views_cnt; ++i) {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
        vkDestroyImageView(device, image_views[i], NULL);
    }
    vkDestroyRenderPass(device, render_pass, NULL);
    vkDestroySwapchainKHR(device, swapchain, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroySemaphore(device, signal_semaphore, NULL);
    vkDestroySemaphore(device, wait_semaphore, NULL);
    vkDestroyFence(device, fence, NULL);
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    vkDestroyCommandPool(device, command_pool, NULL);
    vkDestroyDevice(device, NULL);
    DESTROY_VULKAN_DEBUG_CALLBACK(instance);
    vkDestroyInstance(instance, NULL);
    glfwTerminate();

    return 0;
}
