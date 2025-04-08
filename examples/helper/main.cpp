
#define VIENNA_VULKAN_HELPER_IMPL
#include "VHInclude.h"
#include "helper.h"


namespace vhe {

	auto ToCharPtr(const std::vector<std::string>& vec) -> std::vector<const char*> { 
	    std::vector<const char*> res;
	    for( auto& str : vec) res.push_back(str.c_str());
	    return res;
	}

	void Init( EngineState& engine, WindowState& window, VulkanState& vulkan, SceneState& scene ) {
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


	bool PrepareNextFrame(EngineState& engine, WindowState& window, VulkanState& vulkan, SceneState& scene ) {
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
		return true;
	}

	bool RecordNextFrame(EngineState& engine, WindowState& window, VulkanState& vulkan, SceneState& scene ) {
	    if(window.m_isMinimized) return false;

	    vkResetCommandBuffer(vulkan.m_commandBuffers[vulkan.m_currentFrame],  0);

	    vh::ComStartRecordCommandBuffer(vulkan.m_commandBuffers[vulkan.m_currentFrame], vulkan.m_imageIndex, 
	        vulkan.m_swapChain, vulkan.m_renderPass, 
	        true, 
	        window.m_clearColor, 
	        vulkan.m_currentFrame);

	    vh::ComEndRecordCommandBuffer(vulkan.m_commandBuffers[vulkan.m_currentFrame]);

	    return true;
	}

	bool RenderNextFrame(EngineState& engine, WindowState& window, VulkanState& vulkan, SceneState& scene ) {
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
	    return true;
	}

	void Step( EngineState& engine, WindowState& window, VulkanState& vulkan, SceneState& scene  ) {
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

	        PrepareNextFrame(engine, window, vulkan, scene);
	        RecordNextFrame(engine, window, vulkan, scene);
	        RenderNextFrame(engine, window, vulkan, scene);
	    }
	}


	void Quit(EngineState& engine, WindowState& window, VulkanState& vulkan, SceneState& scene ) {
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
	
		SDL_DestroyWindow(window.m_window);
		SDL_Quit();
	}


}; //namespace vhe


int main() {
	using namespace vhe; 

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

    Init(engine, window, vulkan, scene);

    auto prev = std::chrono::high_resolution_clock::now();
    while (engine.m_running) {
        auto now = std::chrono::high_resolution_clock::now();
		engine.m_dt = std::chrono::duration<double, std::micro>(now - prev).count() / 1'000'000.0;
        prev = now;
        Step(engine, window, vulkan, scene);
    };

	Quit(engine, window, vulkan, scene);

    return 0;
}

