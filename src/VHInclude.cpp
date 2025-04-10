/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>
#include <unordered_map>

#include "VHInclude.h"


namespace vh
{

	extern VkInstance volkInstance;

	auto LoadVolk(const char* name, void* context) {
   		return vkGetInstanceProcAddr(volkInstance, name);
	}

    //------------------------------------------------------------------------

    void SetupImgui(SDL_Window* sdlWindow, VkInstance instance, VkPhysicalDevice physicalDevice, QueueFamilyIndices queueFamilies
        , VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkDescriptorPool descriptorPool
        , VkRenderPass renderPass) {
            
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

        ImGui_ImplVulkan_LoadFunctions( VK_API_VERSION_1_1, &LoadVolk );

        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForVulkan(sdlWindow);
        
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = instance;
        init_info.PhysicalDevice = physicalDevice;
        init_info.Device = device;
        init_info.QueueFamily = queueFamilies.graphicsFamily.value();
        init_info.Queue = graphicsQueue;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = descriptorPool;
        init_info.RenderPass = renderPass;
        init_info.Subpass = 0;
        init_info.MinImageCount = 3;
        init_info.ImageCount = 3;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = nullptr;
        ImGui_ImplVulkan_Init(&init_info);
        // (this gets a bit more complicated, see example app for full reference)
        //ImGui_ImplVulkan_CreateFontsTexture(YOUR_COMMAND_BUFFER);
        // (your code submit a queue)
        //ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    
    bool SDL3Init( std::string name, int width, int height, std::vector<std::string>& extensions) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init SDL: %s", SDL_GetError());
            SDL_Quit();
            return EXIT_FAILURE;
        }
    
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        SDL_Window *window = SDL_CreateWindow( name.c_str(), width, height, window_flags);
        if (!window) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create SDL window: %s", SDL_GetError());
            SDL_Quit();
            return EXIT_FAILURE;
        }
    
        unsigned int sdlExtensionCount = 0;
        const char * const *instance_extensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
        if (instance_extensions == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get SDL Vulkan instance extensions: %s", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return EXIT_FAILURE;
        }
    
        return true;
    }



} // namespace vh



