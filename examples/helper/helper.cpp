
#define VIENNA_VULKAN_HELPER_IMPL
#include "VHInclude.h"
#include <iostream>

const std::vector<const char*> m_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

std::vector<const char*> m_sdl_instance_extensions = {};

#ifdef __APPLE__
    const std::vector<const char*> m_deviceExtensions = {
        "VK_KHR_swapchain",
        "VK_KHR_portability_subset"
};
#else
    const std::vector<const char*> m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
#endif

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

struct Object {
    //vh::UniformBufferObject m_ubo; //holds model, view and proj matrix
    //vh::UniformBuffers m_uniformBuffers;
    //vh::Texture m_texture;
    vh::Mesh m_geometry;
    std::vector<VkDescriptorSet> m_descriptorSets;
};


struct SyncObjects {
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence>     m_inFlightFences;
} m_syncObjects;



struct VulkanState {
    VmaAllocator m_vmaAllocator;

    SDL_Window* m_sdlWindow{nullptr};
    bool m_isMinimized = false;
    bool m_quit = false;

    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkSurfaceKHR m_surface;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device;
    vh::QueueFamilyIndices m_queueFamilies;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkRenderPass m_renderPass;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<Object> m_objects;
    uint32_t m_currentFrame = 0;
    bool m_framebufferResized = false;
} vkState;


void mainLoop() {
    SDL_Event event;
    SDL_PollEvent(&event);

    while (!vkState.m_quit) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event); // Forward your event to backend
            switch (event.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    vkState.m_quit = true;
                    break;

                case SDL_EVENT_WINDOW_MINIMIZED:
                    vkState.m_isMinimized = true;
                    break;

                case SDL_EVENT_WINDOW_MAXIMIZED:
                    vkState.m_isMinimized = false;
                    break;

                case SDL_EVENT_WINDOW_RESTORED:
                    vkState.m_isMinimized = false;
                    break;
            }
        }

        if(!vkState.m_isMinimized) {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            ImGui::ShowDemoWindow(); // Show demo window! :)

            //drawFrame(m_sdlWindow, m_surface, m_physicalDevice, m_device, m_vmaAllocator
            //    , m_graphicsQueue, m_presentQueue, m_swapChain, m_depthImage
            //    , m_renderPass, m_graphicsPipeline, m_objects, m_commandBuffers
            //    , m_syncObjects, m_currentFrame, m_framebufferResized);
        }
    }
    vkDeviceWaitIdle(vkState.m_device);
}


int main() {

    std::vector<std::string> extensions_str;
    vh::SDL3Init( std::string("Vienna Vulkan Helper"), 800, 600, extensions_str);
    std::vector<const char*> extensions;
    for( auto& str : extensions_str) extensions.push_back(str.c_str());

    std::cout << "Hello world\n";
    return 0;
}

