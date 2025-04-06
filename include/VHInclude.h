#pragma once

#include <iostream>
#include <iomanip>

#ifdef VIENNA_VULKAN_HELPER_IMPL
	#define STB_IMAGE_IMPLEMENTATION
	#define STB_IMAGE_WRITE_IMPLEMENTATION
	#define VOLK_IMPLEMENTATION
	#define VMA_IMPLEMENTATION
	#define IMGUI_IMPL_VULKAN_USE_VOLK
#endif

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>
#include <stb_image_write.h>
#include "volk.h"
#include <VkBootstrap.h>
#include "vma/vk_mem_alloc.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "VHVulkan.h"


namespace vh {

	std::vector<char> ReadFile(const std::string& filename);

	void SetupImgui(SDL_Window* sdlWindow, VkInstance instance, VkPhysicalDevice physicalDevice, QueueFamilyIndices queueFamilies
		, VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkDescriptorPool descriptorPool
		, VkRenderPass renderPass);

	bool SDL3Init( std::string name, int width, int height);

};
