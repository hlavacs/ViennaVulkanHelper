
#define VIENNA_VULKAN_HELPER_IMPL
#include "VHInclude.h"
#include "helper.h"


namespace vhe {

	void Init( State& state ) {
	    vh::SDL3Init( std::string("Vienna Vulkan Helper"), 800, 600, state.vulkan.m_instanceExtensions);
	    if (state.engine.m_debug) { state.vulkan.m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); }

	    volkInitialize();
	    state.vulkan.m_apiVersionInstance = state.engine.m_apiVersion;
	    vh::DevCreateInstance( {
	            .m_validationLayers 	= state.vulkan.m_validationLayers, 
	            .m_instanceExtensions 	= state.vulkan.m_instanceExtensions, 
	            .m_name 				= state.engine.m_name, 
	            .m_apiVersion 			= state.vulkan.m_apiVersionInstance, 
	            .m_debug 				= state.engine.m_debug, 
	            .m_instance 			= state.vulkan.m_instance 
	        }
	    );
	
	    volkLoadInstance(state.vulkan.m_instance);

	    if (state.engine.m_debug) {
	        vh::DevSetupDebugMessenger(state.vulkan.m_instance, state.vulkan.m_debugMessenger);
	    }

	    if (SDL_Vulkan_CreateSurface(state.window.m_window, state.vulkan.m_instance, nullptr, &state.vulkan.m_surface) == 0) {
	        printf("Failed to create Vulkan surface.\n");
	    }

	    state.vulkan.m_apiVersionDevice = state.engine.m_minimumVersion;
	    
		vh::DevPickPhysicalDevice(state.vulkan);
		
		uint32_t minor = std::min( VK_VERSION_MINOR(state.vulkan.m_apiVersionDevice), VK_VERSION_MINOR(state.engine.m_apiVersion) );
	    if( minor < VK_VERSION_MINOR(state.engine.m_minimumVersion) ) {
	        std::cout << "No device found with Vulkan API version at least 1." << VK_VERSION_MINOR(state.engine.m_minimumVersion) << "!\n";
	        exit(1);
	    }
	    state.vulkan.m_apiVersion = VK_MAKE_VERSION( VK_VERSION_MAJOR(state.engine.m_apiVersion), minor, 0);
	    state.engine.m_apiVersion = state.vulkan.m_apiVersion;
	    vkGetPhysicalDeviceProperties(state.vulkan.m_physicalDevice, &state.vulkan.m_physicalDeviceProperties);
	    vkGetPhysicalDeviceFeatures(state.vulkan.m_physicalDevice, &state.vulkan.m_physicalDeviceFeatures);

	    state.vulkan.m_depthFormat = vh::ImgPickDepthMapFormat( {
				.m_physicalDevice = state.vulkan.m_physicalDevice, 
				.m_depthFormats = {VK_FORMAT_R32_UINT}
			});

	    vh::DevCreateLogicalDevice( {
			.m_surface 			= state.vulkan.m_surface, 
			.m_physicalDevice 	= state.vulkan.m_physicalDevice, 
			.m_validationLayers = state.vulkan.m_validationLayers, 
	        .m_deviceExtensions = state.vulkan.m_deviceExtensions, 
			.m_debug 			= state.engine.m_debug, 
			.m_queueFamilies 	= state.vulkan.m_queueFamilies, 
			.m_device 			= state.vulkan.m_device, 
			.m_graphicsQueue 	= state.vulkan.m_graphicsQueue, 
			.m_presentQueue 	= state.vulkan.m_presentQueue
		});
	
	    volkLoadDevice(state.vulkan.m_device);
	
	    vh::DevInitVMA(state.vulkan);  

	    vh::DevCreateSwapChain({
			.m_window 			= state.window.m_window, 
			.m_surface 			= state.vulkan.m_surface, 
			.m_physicalDevice 	= state.vulkan.m_physicalDevice, 
			.m_device 			= state.vulkan.m_device, 
			.m_swapChain 		= state.vulkan.m_swapChain
		});
		
	    vh::DevCreateImageViews(state.vulkan);

	    vh::RenCreateRenderPass({
			.m_depthFormat 	= state.vulkan.m_depthFormat, 
			.m_device 		= state.vulkan.m_device, 
			.m_swapChain 	= state.vulkan.m_swapChain, 
			.m_clear 		= true, 
			.m_renderPass 	= state.vulkan.m_renderPass
		});

	    vh::RenCreateDescriptorSetLayout( {
			.m_device = state.vulkan.m_device, 
			.m_bindings = {}, 
			.m_descriptorSetLayout = state.vulkan.m_descriptorSetLayoutPerFrame 
		});
		
	    state.vulkan.m_pipelines.resize(1);
	    vh::RenCreateGraphicsPipeline({
			.m_device = state.vulkan.m_device, 
			.m_renderPass = state.vulkan.m_renderPass, 
			.m_vertShaderPath = "shaders/shader.spv", 
			.m_fragShaderPath = "shaders/shader.spv", 
			.m_bindingDescription = {}, 
			.m_attributeDescriptions = {},
	        .m_descriptorSetLayouts = { state.vulkan.m_descriptorSetLayoutPerFrame }, 
	        .m_specializationConstants = {}, 
	        .m_pushConstantRanges = {}, 
	        .m_blendAttachments = {}, 
	        .m_graphicsPipeline = state.vulkan.m_pipelines[0]
		});

	    state.vulkan.m_commandPools.resize(MAX_FRAMES_IN_FLIGHT);
	    for( int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
	        vh::ComCreateCommandPool( {
				.m_surface = state.vulkan.m_surface, 
				.m_physicalDevice = state.vulkan.m_physicalDevice, 
				.m_device = state.vulkan.m_device, 
				.m_commandPool = state.vulkan.m_commandPools[i]
			});
	    }
	
	    vh::RenCreateDepthResources(state.vulkan);

	    vh::ImgTransitionImageLayout({
			.m_device = state.vulkan.m_device, 
			.m_graphicsQueue = state.vulkan.m_graphicsQueue, 
			.m_commandPool = state.vulkan.m_commandPools[0],
	        .m_image = state.vulkan.m_depthImage.m_depthImage, 
			.m_format = state.vulkan.m_swapChain.m_swapChainImageFormat, 
	        .m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT, 
			.m_mipLevels = 1, 
			.m_layers = 1, 
			.m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, 
			.m_newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		});
		
	    for( auto image : state.vulkan.m_swapChain.m_swapChainImages ) {
	        vh::ImgTransitionImageLayout2({
				.m_device = state.vulkan.m_device, 
				.m_graphicsQueue = state.vulkan.m_graphicsQueue, 
				.m_commandPool = state.vulkan.m_commandPools[0],
	            .m_image = image, 
				.m_format = state.vulkan.m_swapChain.m_swapChainImageFormat, 
	            .m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, 
				.m_newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			});
	    }

	    vh::RenCreateFramebuffers(state.vulkan);
	    vh::RenCreateDescriptorPool( { 
			.m_device = state.vulkan.m_device, 
			.m_sizes = 1000, 
			.m_descriptorPool = state.vulkan.m_descriptorPool 
		});
	
	    vh::SynCreateSemaphores({
			.m_device 					= state.vulkan.m_device, 
			.m_imageAvailableSemaphores = state.vulkan.m_imageAvailableSemaphores, 
			.m_renderFinishedSemaphores = state.vulkan.m_renderFinishedSemaphores, 
			.m_size 					= 3, 
			.m_intermediateSemaphores = state.vulkan.m_intermediateSemaphores
		});

	    vh::SynCreateFences( { state.vulkan.m_device, MAX_FRAMES_IN_FLIGHT, state.vulkan.m_fences });
	}


	bool PrepareNextFrame(State& state) {
	    if(state.window.m_isMinimized) return false;

	    state.vulkan.m_currentFrame = (state.vulkan.m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	    state.vulkan.m_commandBuffers.resize(1);
	    vh::ComCreateCommandBuffers({state.vulkan.m_device, state.vulkan.m_commandPools[state.vulkan.m_currentFrame], state.vulkan.m_commandBuffers});

	    vkWaitForFences(state.vulkan.m_device, 1, &state.vulkan.m_fences[state.vulkan.m_currentFrame], VK_TRUE, UINT64_MAX);

	    VkResult result = vkAcquireNextImageKHR(state.vulkan.m_device, state.vulkan.m_swapChain.m_swapChain, UINT64_MAX,
	                        state.vulkan.m_imageAvailableSemaphores[state.vulkan.m_currentFrame], VK_NULL_HANDLE, &state.vulkan.m_imageIndex);

	    vh::ImgTransitionImageLayout2({
			.m_device 			= state.vulkan.m_device, 
			.m_graphicsQueue 	= state.vulkan.m_graphicsQueue, 
			.m_commandPool 		= state.vulkan.m_commandPools[0], 
	        .m_image 			= state.vulkan.m_swapChain.m_swapChainImages[state.vulkan.m_imageIndex], 
			.m_format 			= state.vulkan.m_swapChain.m_swapChainImageFormat, 
	        .m_oldLayout 		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 
			.m_newLayout 		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		});

	    if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
	        vh::DevRecreateSwapChain( {
				.m_window 			= state.window.m_window, 
	            .m_surface 			= state.vulkan.m_surface, 
				.m_physicalDevice 	= state.vulkan.m_physicalDevice, 
				.m_device 			= state.vulkan.m_device, 
				.m_vmaAllocator 	= state.vulkan.m_vmaAllocator, 
	            .m_swapChain 		= state.vulkan.m_swapChain, 
				.m_depthImage 		= state.vulkan.m_depthImage, 
				.m_renderPass 		= state.vulkan.m_renderPass
			});

	        //m_engine.SendMsg( MsgWindowSize{} );
	    } else assert (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
		return true;
	}

	bool RecordNextFrame(State& state ) {
	    if(state.window.m_isMinimized) return false;

	    vkResetCommandBuffer(state.vulkan.m_commandBuffers[state.vulkan.m_currentFrame],  0);

		vh::ComBeginCommandBuffer({state.vulkan.m_commandBuffers[state.vulkan.m_currentFrame]});

	    vh::ComBeginRenderPass({
			.m_commandBuffer= state.vulkan.m_commandBuffers[state.vulkan.m_currentFrame], 
			.m_imageIndex 	= state.vulkan.m_imageIndex, 
	        .m_swapChain 	= state.vulkan.m_swapChain, 
			.m_renderPass 	= state.vulkan.m_renderPass, 
	        .m_clear 		= true, 
	        .m_clearColor 	= state.window.m_clearColor, 
	        .m_currentFrame = state.vulkan.m_currentFrame
		});

	    vh::ComEndRenderPass({state.vulkan.m_commandBuffers[state.vulkan.m_currentFrame]});
	    vh::ComEndCommandBuffer({state.vulkan.m_commandBuffers[state.vulkan.m_currentFrame]});

	    return true;
	}

	bool RenderNextFrame(State& state) {
	    if(state.window.m_isMinimized) return false;
	
	    vh::ComSubmitCommandBuffers(state.vulkan);

	    vh::ImgTransitionImageLayout2({
			.m_device = state.vulkan.m_device, 
			.m_graphicsQueue = state.vulkan.m_graphicsQueue, 
			.m_commandPool = state.vulkan.m_commandPools[0], 
	        .m_image = state.vulkan.m_swapChain.m_swapChainImages[state.vulkan.m_imageIndex], 
			.m_format = state.vulkan.m_swapChain.m_swapChainImageFormat, 
	        .m_oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
			.m_newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		});

	    VkResult result = vh::ComPresentImage( { 
			.m_presentQueue = state.vulkan.m_presentQueue, 
			.m_swapChain = state.vulkan.m_swapChain, 
	        .m_imageIndex = state.vulkan.m_imageIndex, 
			.m_signalSemaphore = state.vulkan.m_renderFinishedSemaphores[state.vulkan.m_currentFrame]
		});

	    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || state.vulkan.m_framebufferResized) {
	        state.vulkan.m_framebufferResized = false;
	        vh::DevRecreateSwapChain( {
				.m_window = state.window.m_window, 
	            .m_surface = state.vulkan.m_surface, 
				.m_physicalDevice = state.vulkan.m_physicalDevice, 
				.m_device = state.vulkan.m_device, 
				.m_vmaAllocator = state.vulkan.m_vmaAllocator, 
	            .m_swapChain = state.vulkan.m_swapChain, 
				.m_depthImage = state.vulkan.m_depthImage, 
				.m_renderPass = state.vulkan.m_renderPass
			});

	    } else assert(result == VK_SUCCESS);
	    return true;
	}

	void Step( State& state ) {
	    SDL_Event event;
	    SDL_PollEvent(&event);

	    while (SDL_PollEvent(&event)) {
	        ImGui_ImplSDL3_ProcessEvent(&event); // Forward your event to backend
	        switch (event.type) {
	            case SDL_EVENT_QUIT:
	            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
	                state.engine.m_running = false;
	                break;
	            case SDL_EVENT_WINDOW_MINIMIZED:
	                state.window.m_isMinimized = true;
	                break;
	            case SDL_EVENT_WINDOW_MAXIMIZED:
	                state.window.m_isMinimized = false;
	                break;
	            case SDL_EVENT_WINDOW_RESTORED:
	                state.window.m_isMinimized = false;
	                break;
				default: {
						for( auto& system : state.engine.m_systems ) system->Event(state);
					}
					break;
	        }
	    }

		for( auto& system : state.engine.m_systems ) system->Update(state);

	    if(!state.window.m_isMinimized) {
	        ImGui_ImplVulkan_NewFrame();
	        ImGui_ImplSDL3_NewFrame();
	        ImGui::NewFrame();

			for( auto& system : state.engine.m_systems ) system->ImGUI(state);

			ImGui::ShowDemoWindow(); // Show demo window! :)

	        PrepareNextFrame(state);
	        RecordNextFrame(state);
	        RenderNextFrame(state);
	    }
	}


	void Quit(vhe::State& state ) {
		vkDeviceWaitIdle(state.vulkan.m_device);

		state.scene.m_root = nullptr; //clear all objects

		vh::DevCleanupSwapChain(state.vulkan);
	
		for( auto& pipe : state.vulkan.m_pipelines) {
			vkDestroyPipeline(state.vulkan.m_device, pipe.m_pipeline, nullptr);
			vkDestroyPipelineLayout(state.vulkan.m_device, pipe.m_pipelineLayout, nullptr);
		}
	
		vkDestroyDescriptorPool(state.vulkan.m_device, state.vulkan.m_descriptorPool, nullptr);
	
		vkDestroyDescriptorSetLayout(state.vulkan.m_device, state.vulkan.m_descriptorSetLayoutPerFrame, nullptr);
	
		for( auto& pool : state.vulkan.m_commandPools) {
			vkDestroyCommandPool(state.vulkan.m_device, pool, nullptr);
		}
	
		vkDestroyRenderPass(state.vulkan.m_device, state.vulkan.m_renderPass, nullptr);
		vh::SynDestroyFences(state.vulkan);
		vh::SynDestroySemaphores(state.vulkan);
		vmaDestroyAllocator(state.vulkan.m_vmaAllocator);
		vkDestroyDevice(state.vulkan.m_device, nullptr);
		vkDestroySurfaceKHR(state.vulkan.m_instance, state.vulkan.m_surface, nullptr);
	
		if (state.engine.m_debug) {
			vh::DevDestroyDebugUtilsMessengerEXT(state.vulkan);
		}
	
		vkDestroyInstance(state.vulkan.m_instance, nullptr);
	
		SDL_DestroyWindow(state.window.m_window);
		SDL_Quit();
	}


}; //namespace vhe


class MyGame : public vhe::System {
	public:
	MyGame() : System() {};
	~MyGame() {};
	
	virtual void Init(vhe::State& state) {
		//do something
	}

	virtual void FrameStart(vhe::State& state) {
		//do something
	}

	virtual void Event(vhe::State& state) {
		//do something
	};

	virtual void Update(vhe::State& state) {
		//do something
	};

	virtual void ImGUI(vhe::State& state ) {
		//do something
	};

	virtual void FrameEnd(vhe::State& state) {
		//do something
	}
};


int main() {
	using namespace vhe; 

    vhe::State state;

    #ifdef NDEBUG
        state.engine.m_debug = false;
    #else
        state.engine.m_debug = true;
        state.vulkan.m_validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    #endif

    #ifdef __APPLE__
        state.vulkan.m_deviceExtensions.push_back("VK_KHR_portability_subset");
    #endif

	std::vector<std::unique_ptr<System>> systems;
	state.engine.m_systems.emplace_back(std::move(std::make_unique<MyGame>()));

    Init(state);

	for( auto& system : state.engine.m_systems ) system->Init(state);

    auto prev = std::chrono::high_resolution_clock::now();
    while (state.engine.m_running) {
        auto now = std::chrono::high_resolution_clock::now();
		state.engine.m_dt = std::chrono::duration<double, std::micro>(now - prev).count() / 1'000'000.0;
        prev = now;
        Step(state);
    };

	Quit(state);

    return 0;
}

