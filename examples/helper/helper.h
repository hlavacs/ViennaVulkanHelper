#pragma once

#include <iostream>
#include <map>

namespace vhe {

	struct Image {
		VkExtent2D 		m_extent;
		int				m_layers;
		VkDeviceSize	m_size;
		void *			m_pixels{nullptr};
		VkImage         m_image;
	    VmaAllocation   m_allocation;
	    VkImageView     m_view;
	    VkSampler       m_sampler;
	};

	struct Buffer {
		VkDeviceSize 							m_size{0};
	    std::array<VkBuffer, MAXINFLIGHT>       m_buffers;
	    std::array<VmaAllocation, MAXINFLIGHT>  m_allocation;
	    std::array<void*, MAXINFLIGHT>          m_mapped;
	};

	struct DescriptorSet {
		int m_number{0};
		std::array<VkDescriptorSet, MAXINFLIGHT> m_set;
	};

	struct SwapChain {
	    VkSwapchainKHR 	m_swapChain;
	    VkFormat 		m_format;
	    VkExtent2D		m_extent;   
		std::array<VkImage, MAXINFLIGHT+1>		m_images;
	    std::array<VkImageView, MAXINFLIGHT+1> 	m_views;
	    std::array<VkFramebuffer, MAXINFLIGHT> 	m_framebuffers;
	};

	struct Pipeline {
	    VkPipelineLayout 	m_layout;
	    VkPipeline 			m_pipeline;
	};


	 /// @brief Semaphores for signalling that a command buffer has finished executing. Every buffer gets its own Semaphore.
	struct Semaphores {
	    std::vector<VkSemaphore> m_renderFinishedSemaphores;
	};


	/// Pipeline code:
	/// P...Vertex data contains positions
	/// N...Vertex data contains normals
	/// T...Vertex data contains tangents
	/// C...Vertex data contains colors
	/// U...Vertex data contains texture UV coordinates
	struct VertexData {

		static const int size_pos = sizeof(glm::vec3);
		static const int size_nor = sizeof(glm::vec3);
		static const int size_tex = sizeof(glm::vec2);
		static const int size_col = sizeof(glm::vec4);
		static const int size_tan = sizeof(glm::vec3);

		std::vector<glm::vec3> m_positions;
		std::vector<glm::vec3> m_normals;
		std::vector<glm::vec2> m_texCoords;
		std::vector<glm::vec4> m_colors;
		std::vector<glm::vec3> m_tangents;

		auto Type() -> std::string;
		auto Size() -> VkDeviceSize;
		auto Size(std::string type) -> VkDeviceSize;
		auto Offsets() -> std::vector<VkDeviceSize>;
		auto Offsets(std::string type) -> std::vector<VkDeviceSize>;
		void Data(void* data);
		void Data(void* data, std::string type);
	 };

	 struct Mesh {
		VertexData				m_verticesData;
	    std::vector<uint32_t>   m_indices;
	    VkBuffer                m_vertexBuffer;
	    VmaAllocation           m_vertexBufferAllocation;
	    VkBuffer                m_indexBuffer;
	    VmaAllocation           m_indexBufferAllocation;
	};

	class System; 

	 struct EngineState {
	    const std::string m_name;
	    uint32_t    m_apiVersion{VK_API_VERSION_1_1};
	    uint32_t    m_minimumVersion{VK_API_VERSION_1_1};
	    uint32_t    m_maximumVersion{VK_API_VERSION_1_3};
	    bool        m_debug;		
	    bool        m_initialized;
	    bool        m_running;
		std::vector<std::unique_ptr<System>> m_systems;
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
	    VkFormat		m_depthFormat{VK_FORMAT_UNDEFINED};

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

	    uint32_t    m_currentFrame = MAXINFLIGHT - 1;
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

	    ~Object();
	};

	struct SceneState {
	    std::shared_ptr<Object> m_root;
	    std::map<std::string, std::shared_ptr<Object>> m_map{ {"root", {m_root}} };

	};

	struct State {
		EngineState engine;
	    WindowState window;
	    VulkanState vulkan;
	    SceneState scene;
	};

	class System {
		public:
		System() {}
		virtual ~System() {}
		virtual void Init(State& state) = 0;
		virtual void FrameStart(State& state) = 0;
		virtual void Event(State& state) = 0;
		virtual void Update(State& state) = 0;
		virtual void ImGUI(State& state) = 0;
		virtual void FrameEnd(State& state) = 0;
	};

}; //namespace vhe