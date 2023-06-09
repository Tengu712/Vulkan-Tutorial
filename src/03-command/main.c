#include "../common/vulkan-tutorial.h"

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
    // NOTE: キューファミリの中の最初のキューを選択する。
    // NOTE: コマンドを提出する際に必要なので、ここで。
    VkQueue queue;
    vkGetDeviceQueue(device, queue_family_index, 0, &queue);

    // command pool
    // NOTE: コマンドプールを作成する。
    VkCommandPool command_pool;
    {
        const VkCommandPoolCreateInfo ci = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            NULL,
            // NOTE: これを指定すると、各々コマンドバッファをリセットできる。
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            queue_family_index,
        };
        CHECK_VK(vkCreateCommandPool(device, &ci, NULL, &command_pool), "failed to create a command pool.");
    }

    // command buffer
    // NOTE: コマンドバッファを作成する。
    // NOTE: このバッファにコマンドを記録し、バッファごと提出することになる。
    VkCommandBuffer command_buffer;
    {
        const VkCommandBufferAllocateInfo ai = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            NULL,
            command_pool,
            // NOTE: SECONDARYを指定すると、キューには流せないが他のコマンドバッファにコピーできるバッファになる。
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            // NOTE: ここを増やせば一度にアロケートできるが、今回は一つずつアロケートする。
            1,
        };
        CHECK_VK(vkAllocateCommandBuffers(device, &ai, &command_buffer), "failed to allocate a command buffer.");
    }

    // fence
    // NOTE: フェンスを作成する。
    VkFence fence;
    {
        const VkFenceCreateInfo ci = {
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            NULL,
            // NOTE: SIGNALED_BITを指定するとシグナルされた状態で作られる。
            VK_FENCE_CREATE_SIGNALED_BIT,
        };
        CHECK_VK(vkCreateFence(device, &ci, NULL, &fence), "failed to create a fence.");
    }

    // semaphores
    // NOTE: セマフォを作成する。今回は使用しないが、コマンドバッファ関連なのでここで作成しておく。
    // NOTE: wait_semaphoreは本来コマンドバッファ処理前に待機するためのセマフォ。vkAquireNextImageKHR関数に必要で、そこでシグナルにして提出時にアンシグナルにする。
    // NOTE: signal_semaphoreはコマンドバッファ処理後にシグナルされるセマフォ。プレゼンテーションキューのプッシュ時に使う。
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

    // mainloop
    while (1) {
        if (glfwWindowShouldClose(window))
            break;
        glfwPollEvents();

        // prepare
        // NOTE: フェンスがシグナルされるのを待つ。
        WARN_VK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "failed to wait for a fence.");
        // NOTE: フェンスとコマンドバッファをリセットする。
        WARN_VK(vkResetFences(device, 1, &fence), "failed to reset a fence.");
        WARN_VK(vkResetCommandBuffer(command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT), "failed to reset a command buffer.");

        // begin
        // NOTE: コマンドの記録を開始する。
        const VkCommandBufferBeginInfo cmd_bi = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            NULL,
            0,
            NULL,
        };
        WARN_VK(vkBeginCommandBuffer(command_buffer, &cmd_bi), "failed to begin to record commands to render.");

        // end
        // NOTE: コマンド記録を終了する。
        vkEndCommandBuffer(command_buffer);
        // NOTE: キューにサブミットする。
        const VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const VkSubmitInfo submit_info = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            NULL,
            0,
            NULL,
            &wait_stage_mask,
            1,
            &command_buffer,
            0,
            NULL,
        };
        WARN_VK(vkQueueSubmit(queue, 1, &submit_info, fence), "failed to submit commands to queue.");
    }

    // termination
    vkDeviceWaitIdle(device);
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
