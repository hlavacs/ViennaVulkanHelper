#pragma once


namespace vh {

	inline VkInstance volkInstance;

	//---------------------------------------------------------------------------------------------

	inline bool DevCheckValidationLayerSupport(const std::vector<const char*>& validationLayers) {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }


	//---------------------------------------------------------------------------------------------
	void DevPopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    
	//---------------------------------------------------------------------------------------------
	struct DevCreateInstanceInfo {
		const std::vector<const char*>& m_validationLayers;
		const std::vector<const char *>& m_extensions; 
		const std::string& 	m_name; 
		uint32_t& 			m_apiVersion;
		bool& 				m_debug; 
		VkInstance& 		m_instance;
	};

	template<typename T = DevCreateInstanceInfo> 
	inline void DevCreateInstance(T&& info) {
        volkInitialize();

        if (info.m_debug && !DevCheckValidationLayerSupport(info.m_validationLayers)) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = info.m_name.c_str();
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Vienna Vulkan Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(2, 0, 0);
        appInfo.apiVersion = info.m_apiVersion;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        #ifdef __APPLE__
		createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
		#endif

        //auto extensions = requiredExtensions;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(info.m_extensions.size());
        createInfo.ppEnabledExtensionNames = info.m_extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (info.m_debug) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(info.m_validationLayers.size());
            createInfo.ppEnabledLayerNames = info.m_validationLayers.data();

            DevPopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &info.m_instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
        volkInstance = info.m_instance;

   		volkLoadInstance(info.m_instance);
		
		if (vkEnumerateInstanceVersion) {
			VkResult result = vkEnumerateInstanceVersion(&info.m_apiVersion);
		} else {
			info.m_apiVersion = VK_MAKE_VERSION(1, 1, 0);
		}

		std::cout << "Vulkan API Version available on this system: " << info.m_apiVersion <<  
			" Major: " << VK_VERSION_MAJOR(info.m_apiVersion) << 
			" Minor: " << VK_VERSION_MINOR(info.m_apiVersion) << 
			" Patch: " << VK_VERSION_PATCH(info.m_apiVersion) << std::endl;
    }
	
	//---------------------------------------------------------------------------------------------
	struct DevCreateDebugUtilsMessengerEXTInfo {
		const VkInstance& 							m_instance;
		const VkDebugUtilsMessengerCreateInfoEXT*&& m_pCreateInfo;
		const VkAllocationCallbacks*&& 				m_pAllocator;
		VkDebugUtilsMessengerEXT*&& 				m_pDebugMessenger;
	};

	template<typename T = DevCreateDebugUtilsMessengerEXTInfo> 
	inline VkResult DevCreateDebugUtilsMessengerEXT(T&& info) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(info.m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(info.m_instance, info.m_pCreateInfo, info.m_pAllocator, info.m_pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
	
	//---------------------------------------------------------------------------------------------
	struct DevDestroyDebugUtilsMessengerEXTInfo {
		const VkInstance& 				m_instance;
		const VkDebugUtilsMessengerEXT& m_debugMessenger;
		const VkAllocationCallbacks*& 	m_pAllocator;
	};

	template<typename T = DevDestroyDebugUtilsMessengerEXTInfo> 
	inline void DevDestroyDebugUtilsMessengerEXT(T&& info) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(info.m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(info.m_instance, info.m_debugMessenger, info.m_pAllocator);
        }
    }

    //---------------------------------------------------------------------------------------------

	struct DevInitVMAInfo {
		VkInstance& m_instance;
		VkPhysicalDevice& m_physicalDevice;
		VkDevice& m_device;
		uint32_t& m_apiVersion;
		VmaAllocator& m_vmaAllocator;
	};
    
	template<typename T = DevInitVMAInfo>
	inline void DevInitVMA(T&& info) {
        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        allocatorCreateInfo.vulkanApiVersion =  info.m_apiVersion;
        allocatorCreateInfo.physicalDevice = info.m_physicalDevice;
        allocatorCreateInfo.device = info.m_device;
        allocatorCreateInfo.instance = info.m_instance;
        allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
        vmaCreateAllocator(&allocatorCreateInfo, &info.m_vmaAllocator);
    }

	//---------------------------------------------------------------------------------------------
	void DevCleanupSwapChain(VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage);
    
	//---------------------------------------------------------------------------------------------
	void DevRecreateSwapChain(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage, VkRenderPass renderPass);
    

	//---------------------------------------------------------------------------------------------
	void DevSetupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger);
    
	//---------------------------------------------------------------------------------------------
    VKAPI_ATTR VkBool32 VKAPI_CALL DevDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity
        , VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData
        , void* pUserData);

	//---------------------------------------------------------------------------------------------
	void DevCreateSurface(VkInstance instance, SDL_Window *sdlWindow, VkSurfaceKHR& surface);
    
	//---------------------------------------------------------------------------------------------
	void DevPickPhysicalDevice(VkInstance instance, uint32_t& apiVersion, const std::vector<const char*>& deviceExtensions, VkSurfaceKHR surface, VkPhysicalDevice& physicalDevice);

	//---------------------------------------------------------------------------------------------
    void DevCreateLogicalDevice(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, QueueFamilyIndices& queueFamilies, 
		const std::vector<const char*>& validationLayers, const std::vector<const char*>& deviceExtensions, bool debug, 
		VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue);

	//---------------------------------------------------------------------------------------------
    void DevCreateSwapChain(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain);

	//---------------------------------------------------------------------------------------------
    void DevCreateImageViews(VkDevice device, SwapChain& swapChain);

	//---------------------------------------------------------------------------------------------
	VkSurfaceFormatKHR DevChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	//---------------------------------------------------------------------------------------------
    VkPresentModeKHR DevChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

	//---------------------------------------------------------------------------------------------
    VkExtent2D DevChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* sdlWindow);

	//---------------------------------------------------------------------------------------------
    SwapChainSupportDetails DevQuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

	//---------------------------------------------------------------------------------------------
    bool DevIsDeviceSuitable(VkPhysicalDevice device, const std::vector<const char *>&extensions , VkSurfaceKHR surface);

	//---------------------------------------------------------------------------------------------
    bool DevCheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions);

	//---------------------------------------------------------------------------------------------
    QueueFamilyIndices DevFindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);


} // namespace vh
