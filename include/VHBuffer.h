#pragma once


namespace vh {

	//---------------------------------------------------------------------------------------------

    struct BufCreateBufferInfo { 
		const VmaAllocator& 			m_vmaAllocator;
        const VkDeviceSize& 			m_size;
		const VkBufferUsageFlags& 		m_usageFlags;
		const VkMemoryPropertyFlags& 	m_properties;
        const VmaAllocationCreateFlags& m_vmaFlags;
		VkBuffer& 						m_buffer;
        VmaAllocation& 					m_allocation;
		VmaAllocationInfo* 				m_allocationInfo;
	};

	template<typename T = BufCreateBufferInfo>
	inline void BufCreateBuffer(T&& info) {
		VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.size = info.m_size;
		bufferInfo.usage = info.m_usageFlags;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = info.m_vmaFlags;
		vmaCreateBuffer(info.m_vmaAllocator, &bufferInfo, &allocInfo, &info.m_buffer, &info.m_allocation, info.m_allocationInfo);
	}

	//---------------------------------------------------------------------------------------------

    struct BufCreateBuffersInfo {
		const VkDevice& 			m_device;
		const VmaAllocator& 		m_vmaAllocator;
        const VkBufferUsageFlags& 	m_usageFlags;
		const VkDeviceSize& 		m_size;
		Buffer& 					m_buffer;
	};
    
	template<typename T = BufCreateBuffersInfo>
	inline void BufCreateBuffers(T&& info) {

		info.m_buffer.m_bufferSize = info.m_size;
		info.m_buffer.m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		info.m_buffer.m_uniformBuffersAllocation.resize(MAX_FRAMES_IN_FLIGHT);
		info.m_buffer.m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
	
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VmaAllocationInfo allocInfo;
			BufCreateBuffer( {
				.m_vmaAllocator = info.m_vmaAllocator,
				.m_size 		= info.m_size, 
				.m_usageFlags 	= info.m_usageFlags, 
				.m_properties 	= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				.m_vmaFlags 	= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				.m_buffer 		= info.m_buffer.m_uniformBuffers[i],
				.m_allocation 	= info.m_buffer.m_uniformBuffersAllocation[i],
				.m_allocationInfo = &allocInfo
			});

			info.m_buffer.m_uniformBuffersMapped[i] = allocInfo.pMappedData;
		}    
	}

	//---------------------------------------------------------------------------------------------
    struct BufDestroyBufferinfo {
		const VkDevice& 		m_device;
		const VmaAllocator& 	m_vmaAllocator;
		const VkBuffer& 		m_buffer;
		const VmaAllocation& 	m_allocation;
	};

	template<typename T = BufDestroyBufferinfo>
	inline void BufDestroyBuffer(T&& info) {
        vmaDestroyBuffer(info.m_vmaAllocator, info.m_buffer, info.m_allocation);
    }

	//---------------------------------------------------------------------------------------------
    void BufDestroyBuffer2(VkDevice device, VmaAllocator vmaAllocator, Buffer buffers);

	//---------------------------------------------------------------------------------------------
    void BufCopyBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	//---------------------------------------------------------------------------------------------
    void BufCopyBufferToImage(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	//---------------------------------------------------------------------------------------------
    void BufCopyImageToBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkImage image, VkImageAspectFlagBits aspect, VkBuffer buffer, uint32_t layerCount, uint32_t width, uint32_t height);

	//---------------------------------------------------------------------------------------------
	void BufCopyImageToBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkImage image, VkBuffer buffer, std::vector<VkBufferImageCopy> &regions, uint32_t width, uint32_t height);

	//---------------------------------------------------------------------------------------------
    void BufCreateVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Mesh& geometry);

	//---------------------------------------------------------------------------------------------
    void BufCreateIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Mesh& geometry);

  

}; // namespace vh