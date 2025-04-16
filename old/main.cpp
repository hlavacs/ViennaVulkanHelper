#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h> // Include before Vulkan headers if possible

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <VkBootstrap.h> // Needs to be included after volk/vulkan

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <slang.h> // Slang API

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include <stdexcept>
#include <optional>

// --- Constants ---
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const std::string MODEL_PATH = "model.obj";
const std::string TEXTURE_PATH = "texture.png";
const std::string SHADER_NAME = "shader.slang"; // For identification in Slang

// --- Helper Macro ---
#define VK_CHECK(call)                                                         \
    do {                                                                       \
        VkResult result_ = call;                                               \
        if (result_ != VK_SUCCESS) {                                           \
            throw std::runtime_error(std::string("Vulkan error: ") +            \
                                     vkb::to_string(result_) +                  \
                                     " in " + __FILE__ + ":" +                  \
                                     std::to_string(__LINE__));                 \
        }                                                                      \
    } while (0)

// --- Vertex Structure ---
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal; // Included for potential lighting later, good practice
    glm::vec2 uv;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, uv);
        return attributeDescriptions;
    }
};

// --- Allocated Buffer ---
struct AllocatedBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

// --- Allocated Image ---
struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkFormat format;
};

// --- Slang Shader Code ---
const char* slang_shader_code = R"(
// shader.slang
import vulkan;

struct VertexInput {
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
};

struct PixelInput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct UniformData {
    float4x4 mvp;
};

[[vk::binding(0, 0)]] ConstantBuffer<UniformData> ubo; // Set 0, Binding 0
[[vk::binding(1, 0)]] Texture2D textures[1];           // Set 0, Binding 1 (Array for indexing)
[[vk::binding(2, 0)]] SamplerState samplers[1];        // Set 0, Binding 2 (Corresponding Sampler)

// Vertex Shader
[shader("vertex")]
PixelInput vertexMain(VertexInput input) {
    PixelInput output;
    output.position = mul(ubo.mvp, float4(input.pos, 1.0));
    output.uv = input.uv;
    return output;
}

// Fragment Shader
[shader("fragment")]
float4 fragmentMain(PixelInput input) : SV_Target {
    // Access texture via index (descriptor indexing)
    return textures[0].Sample(samplers[0], input.uv);
}
)";

// --- Globals (Minimize where possible, unavoidable for some Vulkan objects) ---
SDL_Window* g_window = nullptr;
vkb::Instance g_vkb_instance;
vkb::Device g_vkb_device;
vkb::Swapchain g_vkb_swapchain;
VkQueue g_graphics_queue = VK_NULL_HANDLE;
uint32_t g_graphics_queue_family = 0;
VmaAllocator g_allocator = VK_NULL_HANDLE;

std::vector<VkImage> g_swapchain_images;
std::vector<VkImageView> g_swapchain_image_views;
VkFormat g_swapchain_image_format;
VkExtent2D g_swapchain_extent;

VkCommandPool g_command_pool = VK_NULL_HANDLE;
VkCommandBuffer g_command_buffer = VK_NULL_HANDLE; // Only one needed for simple case

VkFence g_render_fence = VK_NULL_HANDLE;
VkSemaphore g_present_semaphore = VK_NULL_HANDLE;
VkSemaphore g_render_semaphore = VK_NULL_HANDLE;

AllocatedBuffer g_vertex_buffer;
AllocatedBuffer g_index_buffer;
uint32_t g_index_count = 0;

AllocatedImage g_texture_image;
VkSampler g_texture_sampler = VK_NULL_HANDLE;

AllocatedBuffer g_ubo_buffer;
void* g_ubo_mapped_data = nullptr; // Persistent mapping

VkDescriptorSetLayout g_descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorPool g_descriptor_pool = VK_NULL_HANDLE;
VkDescriptorSet g_descriptor_set = VK_NULL_HANDLE;

VkPipelineLayout g_pipeline_layout = VK_NULL_HANDLE;
VkPipeline g_graphics_pipeline = VK_NULL_HANDLE;

SlangSession* g_slang_session = nullptr; // Slang Global Session

// --- Helper Functions ---

// Slang diagnostic callback
void slangDiagnosticCallback(char const* message, void* userData) {
    std::cerr << "Slang: " << message; // Output errors/warnings
}

// Compile Slang to SPIR-V
std::vector<uint32_t> compileSlangToSpirv(const std::string& source_code, const std::string& entry_point_name, SlangStage stage, const std::string& source_path) {
    if (!g_slang_session) {
        g_slang_session = spCreateSession(nullptr);
        if (!g_slang_session) throw std::runtime_error("Failed to create Slang session");
    }

    spSetDiagnosticCallback(g_slang_session, slangDiagnosticCallback, nullptr);

    SlangCompileRequest* request = spCreateCompileRequest(g_slang_session);
    spSetCodeGenTarget(request, SLANG_SPIRV); // Target SPIR-V

    // Add source file to the request
    int translationUnitIndex = spAddTranslationUnit(request, SLANG_SOURCE_LANGUAGE_SLANG, source_path.c_str());
    spAddTranslationUnitSourceString(request, translationUnitIndex, source_path.c_str(), source_code.c_str());

    // Specify the entry point
    int entryPointIndex = spAddEntryPoint(request, translationUnitIndex, entry_point_name.c_str(), stage);
    if (entryPointIndex < 0) {
        spDestroyCompileRequest(request);
        throw std::runtime_error("Slang: Failed to find entry point '" + entry_point_name + "'");
    }

    // Compile
    int compileResult = spCompile(request);
    const char* diagnostics = spGetDiagnosticOutput(request);
    if (diagnostics && diagnostics[0] != '\0') {
        std::cerr << "Slang compilation diagnostics:\n" << diagnostics << std::endl;
    }

    if (SLANG_FAILED(compileResult)) {
        spDestroyCompileRequest(request);
        throw std::runtime_error("Slang compilation failed");
    }

    // Get SPIR-V code
    size_t dataSize = 0;
    const void* data = spGetEntryPointCode(request, entryPointIndex, &dataSize);
    if (!data || dataSize == 0) {
        spDestroyCompileRequest(request);
        throw std::runtime_error("Slang failed to get compiled SPIR-V code");
    }

    std::vector<uint32_t> spirv_code(dataSize / sizeof(uint32_t));
    memcpy(spirv_code.data(), data, dataSize);

    spDestroyCompileRequest(request);
    return spirv_code;
}


VkShaderModule createShaderModule(const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(g_vkb_device.device, &createInfo, nullptr, &shaderModule));
    return shaderModule;
}

// Create buffer using VMA
AllocatedBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    AllocatedBuffer allocatedBuffer;
    allocatedBuffer.size = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;
    // Add HOST_ACCESS flags if mapping is needed
    if (memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU || memoryUsage == VMA_MEMORY_USAGE_GPU_TO_CPU || memoryUsage == VMA_MEMORY_USAGE_CPU_ONLY) {
         allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // Request persistent mapping
    }


    VK_CHECK(vmaCreateBuffer(g_allocator, &bufferInfo, &allocInfo,
                             &allocatedBuffer.buffer, &allocatedBuffer.allocation,
                             nullptr)); // Use nullptr for pAllocationInfo if mapped bit is set

    return allocatedBuffer;
}

// Helper to create and upload buffer data using a staging buffer
template <typename T>
AllocatedBuffer createAndUploadBuffer(const std::vector<T>& data, VkBufferUsageFlags usage) {
    VkDeviceSize bufferSize = sizeof(T) * data.size();
    if (bufferSize == 0) return {}; // Handle empty data case

    // Create staging buffer (CPU visible)
    AllocatedBuffer stagingBuffer = createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    // Map and copy data to staging buffer
    void* mappedData;
    VK_CHECK(vmaMapMemory(g_allocator, stagingBuffer.allocation, &mappedData));
    memcpy(mappedData, data.data(), bufferSize);
    vmaUnmapMemory(g_allocator, stagingBuffer.allocation);
    // Ensure host writes are visible to device before transfer
    vmaFlushAllocation(g_allocator, stagingBuffer.allocation, 0, VK_WHOLE_SIZE);


    // Create destination buffer (GPU local)
    AllocatedBuffer destBuffer = createBuffer(bufferSize, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    // Copy from staging to destination using a temporary command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = g_command_pool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer tempCmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(g_vkb_device.device, &allocInfo, &tempCmdBuffer));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(tempCmdBuffer, &beginInfo));

    VkBufferCopy copyRegion{};
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(tempCmdBuffer, stagingBuffer.buffer, destBuffer.buffer, 1, ©Region);

    VK_CHECK(vkEndCommandBuffer(tempCmdBuffer));

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &tempCmdBuffer;
    VK_CHECK(vkQueueSubmit(g_graphics_queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(g_graphics_queue)); // Wait for copy to finish

    vkFreeCommandBuffers(g_vkb_device.device, g_command_pool, 1, &tempCmdBuffer);
    vmaDestroyBuffer(g_allocator, stagingBuffer.buffer, stagingBuffer.allocation);

    return destBuffer;
}

// Create image using VMA
AllocatedImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memoryUsage) {
    AllocatedImage allocatedImage;
    allocatedImage.format = format;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    VK_CHECK(vmaCreateImage(g_allocator, &imageInfo, &allocInfo, &allocatedImage.image, &allocatedImage.allocation, nullptr));

    // Create Image View
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = allocatedImage.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(g_vkb_device.device, &viewInfo, nullptr, &allocatedImage.view));

    return allocatedImage;
}

// Transition image layout
void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = g_command_pool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(g_vkb_device.device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(g_graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_graphics_queue);

    vkFreeCommandBuffers(g_vkb_device.device, g_command_pool, 1, &commandBuffer);
}

// Copy buffer to image
void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = g_command_pool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(g_vkb_device.device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, ®ion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(g_graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(g_graphics_queue);

    vkFreeCommandBuffers(g_vkb_device.device, g_command_pool, 1, &commandBuffer);
}

// --- Initialization Functions ---

void initVulkan() {
    // Initialize Volk
    VK_CHECK(volkInitialize());

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error(std::string("SDL could not initialize! SDL_Error: ") + SDL_GetError());
    }
    g_window = SDL_CreateWindow("Vulkan OBJ Viewer", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN);
    if (!g_window) {
        throw std::runtime_error(std::string("SDL window could not be created! SDL_Error: ") + SDL_GetError());
    }

    //--- vk-bootstrap Instance and Device ---
    vkb::InstanceBuilder instance_builder;
    auto inst_ret = instance_builder
        .set_app_name("Vulkan OBJ Viewer")
        .request_validation_layers(true) // Enable validation for debugging
        .require_api_version(1, 3, 0)    // Request Vulkan 1.3
        .use_default_debug_messenger()
        .build();
    if (!inst_ret) throw std::runtime_error("Failed to create Vulkan instance: " + inst_ret.error().message());
    g_vkb_instance = inst_ret.value();

    // Load instance functions using Volk
    volkLoadInstance(g_vkb_instance.instance);

    // Get SDL surface
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(g_window, g_vkb_instance.instance, nullptr, &surface)) {
        throw std::runtime_error(std::string("Failed to create SDL Vulkan surface! SDL_Error: ") + SDL_GetError());
    }

    // vk-bootstrap Physical Device and Logical Device selection
    // Request necessary features
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE; // Enable dynamic rendering
    features13.synchronization2 = VK_TRUE; // Often needed with dynamic rendering

    VkPhysicalDeviceDescriptorIndexingFeatures indexing_features{};
    indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE; // Example feature
    indexing_features.descriptorBindingPartiallyBound = VK_TRUE;
    indexing_features.descriptorBindingVariableDescriptorCount = VK_TRUE; // Often useful
    indexing_features.runtimeDescriptorArray = VK_TRUE;
    indexing_features.descriptorBindingUpdateAfterBind = VK_TRUE; // Core requirement

    vkb::PhysicalDeviceSelector selector{ g_vkb_instance };
    auto phys_ret = selector
        .set_surface(surface)
        .set_minimum_version(1, 3) // Ensure device supports 1.3
        .set_required_features_13(features13)
        .set_required_features_ext<VkPhysicalDeviceDescriptorIndexingFeatures>(indexing_features)
        .add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) // Ensure extension presence
        .add_required_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
        .select();
    if (!phys_ret) throw std::runtime_error("Failed to select Vulkan physical device: " + phys_ret.error().message());

    vkb::DeviceBuilder device_builder{ phys_ret.value() };
    auto dev_ret = device_builder.build();
    if (!dev_ret) throw std::runtime_error("Failed to create Vulkan logical device: " + dev_ret.error().message());
    g_vkb_device = dev_ret.value();

    // Load device functions using Volk
    volkLoadDevice(g_vkb_device.device);

    // Get Graphics Queue
    auto queue_ret = g_vkb_device.get_queue(vkb::QueueType::graphics);
    if (!queue_ret) throw std::runtime_error("Failed to get graphics queue: " + queue_ret.error().message());
    g_graphics_queue = queue_ret.value();
    g_graphics_queue_family = g_vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    //--- VMA Allocator ---
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.physicalDevice = g_vkb_device.physical_device;
    allocatorInfo.device = g_vkb_device.device;
    allocatorInfo.instance = g_vkb_instance.instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT; // Often useful, needed by some extensions
    // Add flag for descriptor indexing / update after bind
    allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT; // Example other flag

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &g_allocator));

    //--- vk-bootstrap Swapchain ---
    vkb::SwapchainBuilder swapchain_builder{ g_vkb_device };
    auto swap_ret = swapchain_builder
        .set_old_swapchain(VK_NULL_HANDLE)
        .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) // Common format
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // VSync
        .set_desired_extent(WINDOW_WIDTH, WINDOW_HEIGHT)
        .add_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .build();
    if (!swap_ret) throw std::runtime_error("Failed to create swapchain: " + swap_ret.error().message());
    g_vkb_swapchain = swap_ret.value();

    g_swapchain_images = g_vkb_swapchain.get_images().value();
    g_swapchain_image_views = g_vkb_swapchain.get_image_views().value();
    g_swapchain_image_format = g_vkb_swapchain.image_format;
    g_swapchain_extent = g_vkb_swapchain.extent;

    //--- Command Pool ---
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = g_graphics_queue_family;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Allow resetting individual buffers
    VK_CHECK(vkCreateCommandPool(g_vkb_device.device, &poolInfo, nullptr, &g_command_pool));

    //--- Command Buffer ---
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = g_command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(g_vkb_device.device, &allocInfo, &g_command_buffer));

    //--- Synchronization Primitives ---
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK(vkCreateSemaphore(g_vkb_device.device, &semaphoreInfo, nullptr, &g_present_semaphore));
    VK_CHECK(vkCreateSemaphore(g_vkb_device.device, &semaphoreInfo, nullptr, &g_render_semaphore));

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled for first frame
    VK_CHECK(vkCreateFence(g_vkb_device.device, &fenceInfo, nullptr, &g_render_fence));
}

void loadAssets() {
    // --- Load Model (tinyobjloader) ---
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
        throw std::runtime_error("Failed to load OBJ: " + warn + err);
    }
    if (!warn.empty()) std::cout << "OBJ Warning: " << warn << std::endl;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices{}; // Simple deduplication

    // Hash function for Vertex (needed for unordered_map) - basic implementation
    auto vertex_hash = [](const Vertex& v) {
        size_t seed = 0;
        auto hash_combine = [&seed](auto const& v) {
           seed ^= std::hash<std::decay_t<decltype(v)>>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };
        hash_combine(v.pos.x); hash_combine(v.pos.y); hash_combine(v.pos.z);
        hash_combine(v.normal.x); hash_combine(v.normal.y); hash_combine(v.normal.z);
        hash_combine(v.uv.x); hash_combine(v.uv.y);
        return seed;
    };
    // Equality for Vertex
     auto vertex_equal = [](const Vertex& a, const Vertex& b) {
        return a.pos == b.pos && a.normal == b.normal && a.uv == b.uv;
     };
     // Use custom hash and equality in the map
     std::unordered_map<Vertex, uint32_t, decltype(vertex_hash), decltype(vertex_equal)> unique_vertices(0, vertex_hash, vertex_equal);


    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            if (index.normal_index >= 0) {
                 vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            } else {
                 vertex.normal = {0.0f, 0.0f, 1.0f}; // Default normal if none provided
            }

            if (index.texcoord_index >= 0) {
                vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1] // Flip Y for Vulkan
                };
            } else {
                vertex.uv = {0.0f, 0.0f}; // Default UV if none provided
            }


            if (unique_vertices.count(vertex) == 0) {
                unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(unique_vertices[vertex]);
        }
    }
    g_index_count = static_cast<uint32_t>(indices.size());

    // Create and Upload Vertex/Index Buffers
    g_vertex_buffer = createAndUploadBuffer(vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    g_index_buffer = createAndUploadBuffer(indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // --- Load Texture (stb_image) ---
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha); // Force RGBA
    if (!pixels) {
        throw std::runtime_error("Failed to load texture image!");
    }
    VkDeviceSize imageSize = texWidth * texHeight * 4; // 4 bytes per pixel (RGBA)

    // Create staging buffer for texture
    AllocatedBuffer stagingBuffer = createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    void* mappedData;
    VK_CHECK(vmaMapMemory(g_allocator, stagingBuffer.allocation, &mappedData));
    memcpy(mappedData, pixels, static_cast<size_t>(imageSize));
    vmaUnmapMemory(g_allocator, stagingBuffer.allocation);
    vmaFlushAllocation(g_allocator, stagingBuffer.allocation, 0, VK_WHOLE_SIZE); // Ensure data is visible

    stbi_image_free(pixels); // Free CPU pixel data

    // Create Texture Image (Device Local)
    g_texture_image = createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, // Use SRGB format for color data
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                  VMA_MEMORY_USAGE_GPU_ONLY);

    // Transition layout, copy, and transition layout again
    transitionImageLayout(g_texture_image.image, g_texture_image.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer.buffer, g_texture_image.image, texWidth, texHeight);
    transitionImageLayout(g_texture_image.image, g_texture_image.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Cleanup staging buffer
    vmaDestroyBuffer(g_allocator, stagingBuffer.buffer, stagingBuffer.allocation);

    // Create Texture Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE; // Enable Anisotropy
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(g_vkb_device.physical_device, &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy; // Use max supported
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; // Or NEAREST if no mipmaps
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f; // No mipmapping in this example

    VK_CHECK(vkCreateSampler(g_vkb_device.device, &samplerInfo, nullptr, &g_texture_sampler));

    // --- Create Uniform Buffer ---
    g_ubo_buffer = createBuffer(sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    // Get the persistently mapped pointer
    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(g_allocator, g_ubo_buffer.allocation, &allocInfo);
    g_ubo_mapped_data = allocInfo.pMappedData;
    if (!g_ubo_mapped_data) { // Fallback if persistent mapping failed/wasn't requested
       VK_CHECK(vmaMapMemory(g_allocator, g_ubo_buffer.allocation, &g_ubo_mapped_data));
       // Note: We won't unmap this until cleanup if mapping wasn't persistent initially.
    }

    // --- Descriptor Set Layout ---
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        // UBO (Binding 0)
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        // Texture Array (Binding 1) - Use Array for indexing demo
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
         // Sampler Array (Binding 2)
        {2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };

    // Set flags for descriptor indexing / update after bind for the texture array binding
    VkDescriptorBindingFlags bindingFlags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |       // Allow unused descriptors in the array
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;      // Allow updating after binding

    std::vector<VkDescriptorBindingFlags> all_binding_flags = {
        0, // Flags for UBO (binding 0)
        bindingFlags, // Flags for Texture Array (binding 1)
        bindingFlags  // Flags for Sampler Array (binding 2) - Needs update after bind too if textures are
    };


    VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{};
    extendedInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    extendedInfo.bindingCount = static_cast<uint32_t>(all_binding_flags.size());
    extendedInfo.pBindingFlags = all_binding_flags.data();


    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT; // Enable update-after-bind for the layout
    layoutInfo.pNext = &extendedInfo; // Link the flags struct

    VK_CHECK(vkCreateDescriptorSetLayout(g_vkb_device.device, &layoutInfo, nullptr, &g_descriptor_set_layout));

    // --- Descriptor Pool ---
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1}, // Pool size for texture array
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1}        // Pool size for sampler array
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolCreateInfo.pPoolSizes = poolSizes.data();
    poolCreateInfo.maxSets = 1;
    poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT; // Enable update-after-bind for the pool

    VK_CHECK(vkCreateDescriptorPool(g_vkb_device.device, &poolCreateInfo, nullptr, &g_descriptor_pool));

    // --- Allocate Descriptor Set ---
    VkDescriptorSetAllocateInfo allocSetInfo{};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = g_descriptor_pool;
    allocSetInfo.descriptorSetCount = 1;
    allocSetInfo.pSetLayouts = &g_descriptor_set_layout;

    VK_CHECK(vkAllocateDescriptorSets(g_vkb_device.device, &allocSetInfo, &g_descriptor_set));

    // --- Update Descriptor Set ---
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = g_ubo_buffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(glm::mat4);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = g_texture_image.view;
    imageInfo.sampler = VK_NULL_HANDLE; // Sampler is separate binding

    VkDescriptorImageInfo samplerInfoDesc{};
    samplerInfoDesc.sampler = g_texture_sampler;
    // ImageView must be null if only updating sampler (or use combined image sampler type)
    samplerInfoDesc.imageView = VK_NULL_HANDLE;
    samplerInfoDesc.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Not relevant for sampler-only


    std::vector<VkWriteDescriptorSet> descriptorWrites(3);

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = g_descriptor_set;
    descriptorWrites[0].dstBinding = 0; // UBO
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = g_descriptor_set;
    descriptorWrites[1].dstBinding = 1; // Texture Array
    descriptorWrites[1].dstArrayElement = 0; // Update the first element
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = g_descriptor_set;
    descriptorWrites[2].dstBinding = 2; // Sampler Array
    descriptorWrites[2].dstArrayElement = 0; // Update the first element
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pImageInfo = &samplerInfoDesc; // Use the sampler-only descriptor info


    vkUpdateDescriptorSets(g_vkb_device.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void createPipeline() {
    // --- Compile Shaders ---
    std::vector<uint32_t> vertSpirv, fragSpirv;
    try {
        vertSpirv = compileSlangToSpirv(slang_shader_code, "vertexMain", SLANG_STAGE_VERTEX, SHADER_NAME);
        fragSpirv = compileSlangToSpirv(slang_shader_code, "fragmentMain", SLANG_STAGE_FRAGMENT, SHADER_NAME);
    } catch (const std::exception& e) {
        std::cerr << "Shader compilation failed: " << e.what() << std::endl;
        throw; // Re-throw after logging
    }

    VkShaderModule vertShaderModule = createShaderModule(vertSpirv);
    VkShaderModule fragShaderModule = createShaderModule(fragSpirv);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "vertexMain"; // Must match Slang entry point name

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "fragmentMain"; // Must match Slang entry point name

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // --- Pipeline Layout ---
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &g_descriptor_set_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 0; // No push constants in this version
    VK_CHECK(vkCreatePipelineLayout(g_vkb_device.device, &pipelineLayoutInfo, nullptr, &g_pipeline_layout));

    // --- Graphics Pipeline ---
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Match tinyobj default + flipped Y
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE; // No blending

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic states: Viewport and Scissor
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // --- Dynamic Rendering Setup ---
    VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &g_swapchain_image_format;
    pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;    // No depth buffer yet
    pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;  // No stencil buffer

    // --- Graphics Pipeline Create Info ---
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &pipelineRenderingCreateInfo; // Chain the dynamic rendering info
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // No depth/stencil state needed without attachment
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = g_pipeline_layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE; // Important: MUST be NULL for dynamic rendering
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VK_CHECK(vkCreateGraphicsPipelines(g_vkb_device.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &g_graphics_pipeline));

    // Cleanup shader modules after pipeline creation
    vkDestroyShaderModule(g_vkb_device.device, fragShaderModule, nullptr);
    vkDestroyShaderModule(g_vkb_device.device, vertShaderModule, nullptr);

    // Cleanup Slang Session
    if (g_slang_session) {
        spDestroySession(g_slang_session);
        g_slang_session = nullptr;
    }
}

void updateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
     model = glm::scale(model, glm::vec3(0.5f)); // Scale down model a bit
    glm::mat4 view = glm::lookAt(glm::vec3(1.5f, 1.5f, 1.5f), glm::vec3(0.0f, 0.0f, 0.2f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), g_swapchain_extent.width / (float)g_swapchain_extent.height, 0.1f, 10.0f);
    proj[1][1] *= -1; // Flip Y for Vulkan clip space

    glm::mat4 mvp = proj * view * model;

    // Copy to mapped buffer
    memcpy(g_ubo_mapped_data, &mvp, sizeof(mvp));
    // No need to flush if memory type is host coherent (common for CPU_TO_GPU VMA usage)
    // If not coherent, you might need:
    // VmaAllocationInfo allocInfo;
    // vmaGetAllocationInfo(g_allocator, g_ubo_buffer.allocation, &allocInfo);
    // vmaFlushAllocation(g_allocator, g_ubo_buffer.allocation, 0, sizeof(mvp));
}


void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Or 0 if reusing
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    // --- Dynamic Rendering Begin ---
    VkRenderingAttachmentInfoKHR colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachmentInfo.imageView = g_swapchain_image_views[imageIndex];
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = g_swapchain_extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = nullptr; // No depth
    renderingInfo.pStencilAttachment = nullptr; // No stencil

    // Image layout transition for the swapchain image (Implicit within vkCmdBeginRendering)
    // Need a barrier if the image isn't already in COLOR_ATTACHMENT_OPTIMAL
    // Note: vkCmdBeginRendering *can* handle the transition, but it's good practice
    // to ensure the layout is correct *before* calling it, especially from PRESENT_SRC.
    // Let's add a manual barrier for clarity.

    VkImageMemoryBarrier image_barrier_to_attachment{};
    image_barrier_to_attachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier_to_attachment.pNext = nullptr;
    image_barrier_to_attachment.srcAccessMask = 0; // Waited on semaphore
    image_barrier_to_attachment.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    image_barrier_to_attachment.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Assume acquired image is undefined or present_src
    image_barrier_to_attachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    image_barrier_to_attachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_to_attachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_to_attachment.image = g_swapchain_images[imageIndex];
    image_barrier_to_attachment.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,           // Wait happens-before
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // Write happens-after
        0, 0, nullptr, 0, nullptr, 1, &image_barrier_to_attachment);


    // Begin dynamic rendering pass
    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    // Bind Pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_graphics_pipeline);

    // Set Dynamic Viewport & Scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(g_swapchain_extent.width);
    viewport.height = static_cast<float>(g_swapchain_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = g_swapchain_extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Bind Vertex Buffer
    VkBuffer vertexBuffers[] = {g_vertex_buffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Bind Index Buffer
    vkCmdBindIndexBuffer(commandBuffer, g_index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    // Bind Descriptor Set (Set 0)
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline_layout, 0, 1, &g_descriptor_set, 0, nullptr);

    // Draw
    vkCmdDrawIndexed(commandBuffer, g_index_count, 1, 0, 0, 0);

    // End Dynamic Rendering
    vkCmdEndRendering(commandBuffer);


    // Transition swapchain image back to Present layout
    VkImageMemoryBarrier image_barrier_to_present{};
    image_barrier_to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_barrier_to_present.pNext = nullptr;
    image_barrier_to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    image_barrier_to_present.dstAccessMask = 0; // Present needs no access mask? Check spec. Often 0.
    image_barrier_to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    image_barrier_to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    image_barrier_to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier_to_present.image = g_swapchain_images[imageIndex];
    image_barrier_to_present.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // Write happened before
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,         // Present waits after
        0, 0, nullptr, 0, nullptr, 1, &image_barrier_to_present);


    // End Command Buffer Recording
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void drawFrame() {
    // Wait for the previous frame to finish rendering
    VK_CHECK(vkWaitForFences(g_vkb_device.device, 1, &g_render_fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(g_vkb_device.device, 1, &g_render_fence)); // Reset fence for current frame

    // Acquire next available image from the swapchain
    uint32_t imageIndex;
    VkResult acquireResult = vkAcquireNextImageKHR(g_vkb_device.device, g_vkb_swapchain.swapchain, UINT64_MAX, g_present_semaphore, VK_NULL_HANDLE, &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
        // Handle swapchain recreation (omitted for brevity in this minimal example)
        // Typically involves destroying old swapchain, recreating it, and potentially pipelines.
        std::cerr << "Swapchain out of date/suboptimal. Needs recreation (not implemented)." << std::endl;
        // For now, just try to continue, might crash or error later.
         // A robust app would call recreateSwapchain() here.
         return; // Exit drawFrame to avoid using invalid resources
    } else if (acquireResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    // Update uniform buffer data for the current frame
    updateUniformBuffer(imageIndex);

    // Reset and record the command buffer
    VK_CHECK(vkResetCommandBuffer(g_command_buffer, 0));
    recordCommandBuffer(g_command_buffer, imageIndex);

    // Submit the command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {g_present_semaphore}; // Wait for image availability
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // Stage to wait at
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_command_buffer;

    VkSemaphore signalSemaphores[] = {g_render_semaphore}; // Signal when rendering finishes
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_CHECK(vkQueueSubmit(g_graphics_queue, 1, &submitInfo, g_render_fence)); // Use fence to signal CPU

    // Present the image to the screen
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores; // Wait for rendering to complete

    VkSwapchainKHR swapChains[] = {g_vkb_swapchain.swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optional

    VkResult presentResult = vkQueuePresentKHR(g_graphics_queue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
         // Handle swapchain recreation (omitted for brevity)
         std::cerr << "Swapchain out of date/suboptimal on present. Needs recreation (not implemented)." << std::endl;
         // recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }
}

void mainLoop() {
    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_EVENT_QUIT) {
                quit = true;
            }
             // Handle window resize events if needed (requires swapchain recreation)
            // if (e.type == SDL_EVENT_WINDOW_RESIZED) {
            //     // Handle resize, recreate swapchain etc.
            // }
        }
        drawFrame();
    }
    // Wait for the device to be idle before cleanup
    vkDeviceWaitIdle(g_vkb_device.device);
}

void cleanup() {
    // Destroy Vulkan objects in reverse order of creation
    vkDestroyPipeline(g_vkb_device.device, g_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(g_vkb_device.device, g_pipeline_layout, nullptr);

    vkDestroyDescriptorPool(g_vkb_device.device, g_descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(g_vkb_device.device, g_descriptor_set_layout, nullptr);

    if (g_ubo_mapped_data) {
        // Unmap if we had to map manually (i.e., not persistently mapped by VMA)
        // Check if the memory type required manual map/unmap. For simplicity, we just unmap if mapped pointer exists
        // A more robust check would involve checking VmaAllocationInfo flags.
        // VmaAllocationInfo allocInfo;
        // vmaGetAllocationInfo(g_allocator, g_ubo_buffer.allocation, &allocInfo);
        // if (!(allocInfo.memoryType & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) || !(allocInfo.memoryType & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        //     // Or simply if persistent map wasn't guaranteed
        //      vmaUnmapMemory(g_allocator, g_ubo_buffer.allocation);
        // }
        // For this example, assuming CPU_TO_GPU with MAPPED_BIT handles it, or we mapped manually.
        // VMA handles unmapping on destruction if mapped via vmaMapMemory
    }
    vmaDestroyBuffer(g_allocator, g_ubo_buffer.buffer, g_ubo_buffer.allocation);

    vkDestroySampler(g_vkb_device.device, g_texture_sampler, nullptr);
    vkDestroyImageView(g_vkb_device.device, g_texture_image.view, nullptr);
    vmaDestroyImage(g_allocator, g_texture_image.image, g_texture_image.allocation);

    vmaDestroyBuffer(g_allocator, g_index_buffer.buffer, g_index_buffer.allocation);
    vmaDestroyBuffer(g_allocator, g_vertex_buffer.buffer, g_vertex_buffer.allocation);

    vkDestroyFence(g_vkb_device.device, g_render_fence, nullptr);
    vkDestroySemaphore(g_vkb_device.device, g_render_semaphore, nullptr);
    vkDestroySemaphore(g_vkb_device.device, g_present_semaphore, nullptr);

    vkDestroyCommandPool(g_vkb_device.device, g_command_pool, nullptr);

    // Swapchain resources are managed by vkb::Swapchain destructor
    g_vkb_swapchain.destroy_image_views(g_swapchain_image_views); // Must be done before destroying swapchain itself if manually created
    vkb::destroy_swapchain(g_vkb_swapchain); // Use vkb helper


    // VMA Allocator
    vmaDestroyAllocator(g_allocator);

    // Logical Device and Instance managed by vkb destructors
    vkb::destroy_device(g_vkb_device);
    vkb::destroy_surface(g_vkb_instance.instance, g_vkb_device.surface); // Destroy surface *before* instance
    vkb::destroy_instance(g_vkb_instance);


    // Destroy SDL Window
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = nullptr;
    }
    SDL_Quit();

     // Slang session cleanup was moved to after shader compilation
}

// --- Main Function ---
int main(int argc, char* argv[]) {
    try {
        initVulkan();
        loadAssets();
        createPipeline();
        mainLoop();
        cleanup();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
         // Ensure cleanup happens even on error
        // Note: A more robust cleanup might be needed depending on where the error occurred.
        // Consider using RAII wrappers for Vulkan objects.
        cleanup(); // Attempt cleanup
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
