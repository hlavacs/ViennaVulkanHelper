
#define VIENNA_VULKAN_HELPER_IMPL
#include "VHInclude.h"
#include <iostream>
#include <map>

struct EngineState {
    const std::string m_name;
    uint32_t    m_apiVersion{VK_API_VERSION_1_1};
    uint32_t    m_minimumVersion{VK_API_VERSION_1_1};
    uint32_t    m_maximumVersion{VK_API_VERSION_1_3};
    bool        m_debug;		
    bool        m_initialized;
    bool        m_running;	
    double      m_dt;
};

struct WindowState {
    int 		  m_width{0};
    int 		  m_height{0};
    std::string   m_windowName{""};
    glm::vec4 	  m_clearColor{0.45f, 0.55f, 0.60f, 1.00f};
    bool 		  m_isMinimized{false};
    bool 		  m_isInitialized{false};
    SDL_Window*   m_window{nullptr};
};

struct VulkanState {
    std::vector<std::string>    m_instanceExtensions{};
    std::vector<std::string>    m_deviceExtensions{"VK_KHR_swapchain"};
    std::vector<std::string>    m_validationLayers{};

    uint32_t 		m_apiVersionInstance{VK_API_VERSION_1_1};
    uint32_t 		m_apiVersionDevice{VK_API_VERSION_1_1};
    uint32_t        m_apiVersion{VK_API_VERSION_1_1};
    VkInstance 		m_instance{VK_NULL_HANDLE};
    VkSurfaceKHR 	m_surface{VK_NULL_HANDLE};
    VmaAllocator 	m_vmaAllocator;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkAllocationCallbacks* m_pAllocator{nullptr};
    
    VkPhysicalDevice 			m_physicalDevice{VK_NULL_HANDLE};
    VkPhysicalDeviceFeatures 	m_physicalDeviceFeatures;
    VkPhysicalDeviceProperties 	m_physicalDeviceProperties;

    VkDevice 		m_device{VK_NULL_HANDLE};
    vh::QueueFamilyIndices m_queueFamilies;
    VkQueue 		m_graphicsQueue{VK_NULL_HANDLE};
    VkQueue 		m_presentQueue{VK_NULL_HANDLE};
    vh::SwapChain 	m_swapChain;
    vh::DepthImage 	m_depthImage;
    VkFormat		m_depthMapFormat{VK_FORMAT_UNDEFINED};

    std::vector<VkCommandPool> m_commandPools; //per frame in flight
    std::vector<VkCommandBuffer> m_commandBuffers; //collect command buffers to submit

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<vh::Semaphores> m_intermediateSemaphores;
    std::vector<VkFence>     m_fences;

    vh::Buffer          m_uniformBuffersPerFrame;
    vh::Buffer          m_uniformBuffersLights;
    VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
    vh::DescriptorSet   m_descriptorSetPerFrame{0};
    VkRenderPass        m_renderPass;
    VkDescriptorPool    m_descriptorPool;

    std::vector<vh::Pipeline> m_pipelines;

    uint32_t    m_currentFrame = MAX_FRAMES_IN_FLIGHT - 1;
    uint32_t    m_imageIndex;
    bool        m_framebufferResized = false;
};

struct Object {
    const VulkanState&  m_vulkan;
    std::string         m_name;
    vh::BufferPerObjectTexture m_ubo; 
    vh::Buffer          m_uniformBuffers;
    vh::Map             m_texture;
    vh::Mesh            m_mesh;
    std::vector<VkDescriptorSet> m_descriptorSets;

    glm::mat4 m_localToParent{1.0f}; //contains position, orientation and scale
    glm::mat4 m_localToWorld{1.0f};

    std::shared_ptr<Object> m_nextSibling{nullptr};
    std::shared_ptr<Object> m_firstChild{nullptr};

    ~Object() {
        vkDestroySampler(m_vulkan.m_device, m_texture.m_mapSampler, nullptr);
        vkDestroyImageView(m_vulkan.m_device, m_texture.m_mapImageView, nullptr);
        vh::ImgDestroyImage(m_vulkan.m_device, m_vulkan.m_vmaAllocator, m_texture.m_mapImage, m_texture.m_mapImageAllocation);
        vh::BufDestroyBuffer(m_vulkan.m_device, m_vulkan.m_vmaAllocator, m_mesh.m_indexBuffer, m_mesh.m_indexBufferAllocation);
        vh::BufDestroyBuffer(m_vulkan.m_device, m_vulkan.m_vmaAllocator, m_mesh.m_vertexBuffer, m_mesh.m_vertexBufferAllocation);
        vh::BufDestroyBuffer2(m_vulkan.m_device, m_vulkan.m_vmaAllocator, m_uniformBuffers);
    };
};

struct SceneState {
    std::shared_ptr<Object> m_root;
    std::map<std::string, std::shared_ptr<Object>> m_map{ {"root", {m_root}} };
};

std::vector<const char*> ToCharPtr(const std::vector<std::string>& vec) { 
    std::vector<const char*> res;
    for( auto& str : vec) res.push_back(str.c_str());
    return res;
}

void Init( EngineState& engine, WindowState& window, VulkanState& vulkan ) {
    vh::SDL3Init( std::string("Vienna Vulkan Helper"), 800, 600, vulkan.m_instanceExtensions);
    if (engine.m_debug) { vulkan.m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); }

    volkInitialize();
    vulkan.m_apiVersionInstance = engine.m_apiVersion;
    vh::DevCreateInstance( {
            .m_validationLayers = ToCharPtr(vulkan.m_validationLayers), 
            .m_extensions = ToCharPtr(vulkan.m_instanceExtensions), 
            .m_name = engine.m_name, 
            .m_apiVersion = vulkan.m_apiVersionInstance, 
            .m_debug = engine.m_debug, 
            .m_instance = vulkan.m_instance 
        }
    );
    
    volkLoadInstance(vulkan.m_instance);

    if (engine.m_debug) {
        vh::DevSetupDebugMessenger(vulkan.m_instance, vulkan.m_debugMessenger);
    }

    if (SDL_Vulkan_CreateSurface(window.m_window, vulkan.m_instance, nullptr, &vulkan.m_surface) == 0) {
        printf("Failed to create Vulkan surface.\n");
    }

    vulkan.m_apiVersionDevice = engine.m_minimumVersion;
    vh::DevPickPhysicalDevice(vulkan.m_instance, vulkan.m_apiVersionDevice, ToCharPtr(vulkan.m_deviceExtensions), vulkan.m_surface, vulkan.m_physicalDevice);
    uint32_t minor = std::min( VK_VERSION_MINOR(vulkan.m_apiVersionDevice), VK_VERSION_MINOR(engine.m_apiVersion) );
    if( minor < VK_VERSION_MINOR(engine.m_minimumVersion) ) {
        std::cout << "No device found with Vulkan API version at least 1." << VK_VERSION_MINOR(engine.m_minimumVersion) << "!\n";
        exit(1);
    }
    vulkan.m_apiVersion = VK_MAKE_VERSION( VK_VERSION_MAJOR(engine.m_apiVersion), minor, 0);
    engine.m_apiVersion = vulkan.m_apiVersion;
    vkGetPhysicalDeviceProperties(vulkan.m_physicalDevice, &vulkan.m_physicalDeviceProperties);
    vkGetPhysicalDeviceFeatures(vulkan.m_physicalDevice, &vulkan.m_physicalDeviceFeatures);
    vh::ImgPickDepthMapFormat(vulkan.m_physicalDevice, {VK_FORMAT_R32_UINT}, vulkan.m_depthMapFormat);

    vh::DevCreateLogicalDevice(vulkan.m_surface, vulkan.m_physicalDevice, vulkan.m_queueFamilies, ToCharPtr(vulkan.m_validationLayers), 
        ToCharPtr(vulkan.m_deviceExtensions), engine.m_debug, vulkan.m_device, vulkan.m_graphicsQueue, vulkan.m_presentQueue);
    
    volkLoadDevice(vulkan.m_device);
    
    vh::DevInitVMA(vulkan);  
    vh::DevCreateSwapChain(window.m_window, 
        vulkan.m_surface, vulkan.m_physicalDevice, vulkan.m_device, vulkan.m_swapChain);
    
    vh::DevCreateImageViews(vulkan.m_device, vulkan.m_swapChain);

    vh::RenCreateRenderPass(vulkan.m_physicalDevice, vulkan.m_device, vulkan.m_swapChain, true, vulkan.m_renderPass);

    vh::RenCreateDescriptorSetLayout( vulkan.m_device, {}, vulkan.m_descriptorSetLayoutPerFrame );
    
    vulkan.m_pipelines.resize(1);
    vh::RenCreateGraphicsPipeline(vulkan.m_device, vulkan.m_renderPass, "shaders/shader.spv", "shaders/shader.spv", {}, {},
         { vulkan.m_descriptorSetLayoutPerFrame }, 
         {}, //spezialization constants
         {}, //push constants
         {}, //blend attachments
         vulkan.m_pipelines[0]);

    vulkan.m_commandPools.resize(MAX_FRAMES_IN_FLIGHT);
    for( int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
        vh::ComCreateCommandPool(vulkan.m_surface, vulkan.m_physicalDevice, vulkan.m_device, vulkan.m_commandPools[i]);
    }
    
    vh::RenCreateDepthResources(vulkan.m_physicalDevice, vulkan.m_device, vulkan.m_vmaAllocator, vulkan.m_swapChain, vulkan.m_depthImage);
    vh::ImgTransitionImageLayout2(vulkan.m_device, vulkan.m_graphicsQueue, vulkan.m_commandPools[0],
        vulkan.m_depthImage.m_depthImage, vulkan.m_swapChain.m_swapChainImageFormat, 
        VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, VK_IMAGE_LAYOUT_UNDEFINED , VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
    for( auto image : vulkan.m_swapChain.m_swapChainImages ) {
        vh::ImgTransitionImageLayout(vulkan.m_device, vulkan.m_graphicsQueue, vulkan.m_commandPools[0],
            image, vulkan.m_swapChain.m_swapChainImageFormat, 
            VK_IMAGE_LAYOUT_UNDEFINED , VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    vh::RenCreateFramebuffers(vulkan.m_device, vulkan.m_swapChain, vulkan.m_depthImage, vulkan.m_renderPass);
    vh::RenCreateDescriptorPool(vulkan.m_device, 1000, vulkan.m_descriptorPool);
    vh::SynCreateSemaphores(vulkan.m_device, vulkan.m_imageAvailableSemaphores, vulkan.m_renderFinishedSemaphores, 3, vulkan.m_intermediateSemaphores);

    vh::SynCreateFences(vulkan.m_device, MAX_FRAMES_IN_FLIGHT, vulkan.m_fences);
}


bool PrepareNextFrame(EngineState& engine, WindowState& window, VulkanState& vulkan) {
    if(window.m_isMinimized) return false;

    vulkan.m_currentFrame = (vulkan.m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    vulkan.m_commandBuffers.resize(1);
    vh::ComCreateCommandBuffers(vulkan.m_device, vulkan.m_commandPools[vulkan.m_currentFrame], vulkan.m_commandBuffers);

    vkWaitForFences(vulkan.m_device, 1, &vulkan.m_fences[vulkan.m_currentFrame], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(vulkan.m_device, vulkan.m_swapChain.m_swapChain, UINT64_MAX,
                        vulkan.m_imageAvailableSemaphores[vulkan.m_currentFrame], VK_NULL_HANDLE, &vulkan.m_imageIndex);

    vh::ImgTransitionImageLayout(vulkan.m_device, vulkan.m_graphicsQueue, vulkan.m_commandPools[0], 
        vulkan.m_swapChain.m_swapChainImages[vulkan.m_imageIndex], vulkan.m_swapChain.m_swapChainImageFormat, 
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
        DevRecreateSwapChain( window.m_window, 
            vulkan.m_surface, vulkan.m_physicalDevice, vulkan.m_device, vulkan.m_vmaAllocator, 
            vulkan.m_swapChain, vulkan.m_depthImage, vulkan.m_renderPass);

        //m_engine.SendMsg( MsgWindowSize{} );
    } else assert (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
    return false;
}

bool RecordNextFrame(EngineState& engine, WindowState& window, VulkanState& vulkan) {
    if(window.m_isMinimized) return false;

    vkResetCommandBuffer(vulkan.m_commandBuffers[vulkan.m_currentFrame],  0);

    vh::ComStartRecordCommandBuffer(vulkan.m_commandBuffers[vulkan.m_currentFrame], vulkan.m_imageIndex, 
        vulkan.m_swapChain, vulkan.m_renderPass, 
        true, 
        window.m_clearColor, 
        vulkan.m_currentFrame);

    vh::ComEndRecordCommandBuffer(vulkan.m_commandBuffers[vulkan.m_currentFrame]);

    return false;
}

bool RenderNextFrame(EngineState& engine, WindowState& window, VulkanState& vulkan) {
    if(window.m_isMinimized) return false;
        
    vh::ComSubmitCommandBuffers(vulkan.m_device, vulkan.m_graphicsQueue, vulkan.m_commandBuffers, 
        vulkan.m_imageAvailableSemaphores, vulkan.m_renderFinishedSemaphores, vulkan.m_intermediateSemaphores, vulkan.m_fences, vulkan.m_currentFrame);

    vh::ImgTransitionImageLayout(vulkan.m_device, vulkan.m_graphicsQueue, vulkan.m_commandPools[0], 
        vulkan.m_swapChain.m_swapChainImages[vulkan.m_imageIndex], vulkan.m_swapChain.m_swapChainImageFormat, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkResult result = vh::ComPresentImage(vulkan.m_presentQueue, vulkan.m_swapChain, 
        vulkan.m_imageIndex, vulkan.m_renderFinishedSemaphores[vulkan.m_currentFrame]);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || vulkan.m_framebufferResized) {
        vulkan.m_framebufferResized = false;
        vh::DevRecreateSwapChain(window.m_window, 
            vulkan.m_surface, vulkan.m_physicalDevice, vulkan.m_device, vulkan.m_vmaAllocator, 
            vulkan.m_swapChain, vulkan.m_depthImage, vulkan.m_renderPass);

    } else assert(result == VK_SUCCESS);
    return false;
}

void Step( EngineState& engine, WindowState& window, VulkanState& vulkan ) {
    SDL_Event event;
    SDL_PollEvent(&event);

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event); // Forward your event to backend
        switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                engine.m_running = false;
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                window.m_isMinimized = true;
                break;
            case SDL_EVENT_WINDOW_MAXIMIZED:
                window.m_isMinimized = false;
                break;
            case SDL_EVENT_WINDOW_RESTORED:
                window.m_isMinimized = false;
                break;
        }
    }
    if(!window.m_isMinimized) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow(); // Show demo window! :)

        PrepareNextFrame(engine, window, vulkan);
        RecordNextFrame(engine, window, vulkan);
        RenderNextFrame(engine, window, vulkan);
    }
}

template void vh::DevDestroyDebugUtilsMessengerEXT<VulkanState>(VulkanState&&);


int main() {
    EngineState engine;
    WindowState window;
    VulkanState vulkan;
    SceneState scene;

    #ifdef NDEBUG
        engine.m_debug = false;
    #else
        engine.m_debug = true;
        vulkan.m_validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    #endif

    #ifdef __APPLE__
        vulkan.m_deviceExtensions.push_back("VK_KHR_portability_subset");
    #endif

    Init(engine, window, vulkan);

    auto prev = std::chrono::high_resolution_clock::now();
    while (engine.m_running) {
        auto now = std::chrono::high_resolution_clock::now();
		engine.m_dt = std::chrono::duration<double, std::micro>(now - prev).count() / 1'000'000.0;
        prev = now;
        Step(engine, window, vulkan);
    };

    vkDeviceWaitIdle(vulkan.m_device);

    scene.m_root = nullptr; //clear all objects

    vh::DevCleanupSwapChain(vulkan.m_device, vulkan.m_vmaAllocator, vulkan.m_swapChain, vulkan.m_depthImage);

    for( auto& pipe : vulkan.m_pipelines) {
        vkDestroyPipeline(vulkan.m_device, pipe.m_pipeline, nullptr);
        vkDestroyPipelineLayout(vulkan.m_device, pipe.m_pipelineLayout, nullptr);
    }

    vkDestroyDescriptorPool(vulkan.m_device, vulkan.m_descriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(vulkan.m_device, vulkan.m_descriptorSetLayoutPerFrame, nullptr);

    for( auto& pool : vulkan.m_commandPools) {
        vkDestroyCommandPool(vulkan.m_device, pool, nullptr);
    }

    vkDestroyRenderPass(vulkan.m_device, vulkan.m_renderPass, nullptr);
    vh::SynDestroyFences(vulkan.m_device, vulkan.m_fences);
    vh::SynDestroySemaphores(vulkan.m_device, vulkan.m_imageAvailableSemaphores, vulkan.m_renderFinishedSemaphores, vulkan.m_intermediateSemaphores);
    vmaDestroyAllocator(vulkan.m_vmaAllocator);
    vkDestroyDevice(vulkan.m_device, nullptr);
    vkDestroySurfaceKHR(vulkan.m_instance, vulkan.m_surface, nullptr);


    if (engine.m_debug) {
        vh::DevDestroyDebugUtilsMessengerEXT(vulkan);
    }

    vkDestroyInstance(vulkan.m_instance, nullptr);
    return 0;
}

