#include "VHInclude.h"
#include "helper.h"

namespace vhe {

    Object::~Object() {
        vkDestroySampler(m_vulkan.m_device, m_texture.m_mapSampler, nullptr);
        vkDestroyImageView(m_vulkan.m_device, m_texture.m_mapImageView, nullptr);
        vh::ImgDestroyImage(m_vulkan.m_device, m_vulkan.m_vmaAllocator, m_texture.m_mapImage, m_texture.m_mapImageAllocation);
        vh::BufDestroyBuffer({m_vulkan.m_device, m_vulkan.m_vmaAllocator, m_mesh.m_indexBuffer, m_mesh.m_indexBufferAllocation});
        vh::BufDestroyBuffer({m_vulkan.m_device, m_vulkan.m_vmaAllocator, m_mesh.m_vertexBuffer, m_mesh.m_vertexBufferAllocation});
        vh::BufDestroyBuffer2({m_vulkan.m_device, m_vulkan.m_vmaAllocator, m_uniformBuffers});
    };
    
    auto VertexData::Type() -> std::string{
        std::string name;
        if( m_positions.size() > 0 ) name = name + "P";
        if( m_normals.size() > 0 )   name = name + "N";
        if( m_texCoords.size() > 0 ) name = name + "U";
        if( m_colors.size() > 0 )    name = name + "C";
        if( m_tangents.size() > 0 )  name = name + "T";
        return name;
    }
    
    auto VertexData::Size() -> VkDeviceSize {
        return 	m_positions.size() * sizeof(glm::vec3) + 
                m_normals.size()   * sizeof(glm::vec3) + 
                m_texCoords.size() * sizeof(glm::vec2) + 
                m_colors.size()    * sizeof(glm::vec4) + 
                m_tangents.size()  * sizeof(glm::vec3);
    }
    
     auto VertexData::Size( std::string type ) -> VkDeviceSize {
        return 	type.find("P") != std::string::npos ? m_positions.size() * sizeof(glm::vec3) : 0 + 
                type.find("N") != std::string::npos ? m_normals.size()   * sizeof(glm::vec3) : 0 + 
                type.find("U") != std::string::npos ? m_texCoords.size() * sizeof(glm::vec2) : 0 + 
                type.find("C") != std::string::npos ? m_colors.size()    * sizeof(glm::vec4) : 0 + 
                type.find("T") != std::string::npos ? m_tangents.size()  * sizeof(glm::vec3) : 0;
    }
    
    auto VertexData::Offsets() -> std::vector<VkDeviceSize> {
        size_t offset=0;
        std::vector<VkDeviceSize> offsets{};
        if( size_t size = m_positions.size() * size_pos; size > 0 ) { offsets.push_back(offset); offset += size; }
        if( size_t size = m_normals.size()   * size_nor; size > 0 ) { offsets.push_back(offset); offset += size; }
        if( size_t size = m_texCoords.size() * size_tex; size > 0 ) { offsets.push_back(offset); offset += size; }
        if( size_t size = m_colors.size()    * size_col; size > 0 ) { offsets.push_back(offset); offset += size; }
        if( size_t size = m_tangents.size()  * size_tan; size > 0 ) { offsets.push_back(offset); offset += size; }
        return offsets;
    }
    
    auto VertexData::Offsets(std::string type) -> std::vector<VkDeviceSize> {
        size_t offset=0;
        std::vector<VkDeviceSize> offsets{};
        if( type.find("P") != std::string::npos ) { offsets.push_back(offset); offset += m_positions.size() * size_pos; }
        if( type.find("N") != std::string::npos ) { offsets.push_back(offset); offset += m_normals.size()   * size_nor; }
        if( type.find("U") != std::string::npos ) { offsets.push_back(offset); offset += m_texCoords.size() * size_tex; }
        if( type.find("C") != std::string::npos ) { offsets.push_back(offset); offset += m_colors.size()    * size_col; }
        if( type.find("T") != std::string::npos ) { offsets.push_back(offset); offset += m_tangents.size()  * size_tan; }
        return offsets;
    }
    
    void VertexData::Data(void* data) {
        size_t offset=0, size = 0;
        size = m_positions.size() * size_pos; memcpy( data, m_positions.data(), size );                 offset += size;
        size = m_normals.size()   * size_nor; memcpy( (char*)data + offset, m_normals.data(), size );   offset += size;
        size = m_texCoords.size() * size_tex; memcpy( (char*)data + offset, m_texCoords.data(), size ); offset += size;
        size = m_colors.size()    * size_col; memcpy( (char*)data + offset, m_colors.data(), size );    offset += size;
        size = m_tangents.size()  * size_tan; memcpy( (char*)data + offset, m_tangents.data(), size );  offset += size;
    }
    
    void VertexData::Data(void* data, std::string type) {
        size_t offset=0, size = 0;
        if( type.find("P") != std::string::npos ) { size = m_positions.size() * size_pos; memcpy( data, m_positions.data(), size ); offset += size; }
        if( type.find("N") != std::string::npos ) { size = m_normals.size()   * size_nor; memcpy( (char*)data + offset, m_normals.data(), size ); offset += size; }
        if( type.find("U") != std::string::npos ) { size = m_texCoords.size() * size_tex; memcpy( (char*)data + offset, m_texCoords.data(), size ); offset += size; }
        if( type.find("C") != std::string::npos ) { size = m_colors.size()    * size_col; memcpy( (char*)data + offset, m_colors.data(), size ); offset += size; }
        if( type.find("T") != std::string::npos ) { size = m_tangents.size()  * size_tan; memcpy( (char*)data + offset, m_tangents.data(), size ); offset += size; }
    }
    
}; //