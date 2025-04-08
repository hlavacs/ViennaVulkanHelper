set(TARGET helper)

set(SOURCE 
	main.cpp
	helper.cpp
	${PROJECT_SOURCE_DIR}/src/VHBuffer.cpp
	${PROJECT_SOURCE_DIR}/src/VHCommand.cpp
	${PROJECT_SOURCE_DIR}/src/VHImage.cpp
	${PROJECT_SOURCE_DIR}/src/VHRender.cpp
	${PROJECT_SOURCE_DIR}/src/VHSync.cpp
	${PROJECT_SOURCE_DIR}/src/VHInclude.cpp
)

set(HEADERS 
	helper.h
	${INCLUDE}/VHBuffer.h
	${INCLUDE}/VHCommand.h
	${INCLUDE}/VHDevice.h
	${INCLUDE}/VHImage.h
	${INCLUDE}/VHRender.h
	${INCLUDE}/VHSync.h
	${INCLUDE}/VHVulkan.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_include_directories(${TARGET} PRIVATE stb)
target_include_directories(${TARGET} PRIVATE ${Vulkan_INCLUDE_DIR}/volk)
target_include_directories(${TARGET} PRIVATE ${Vulkan_INCLUDE_DIR}/vma)
target_include_directories(${TARGET} SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${TARGET} PRIVATE SDL3::SDL3 vk-bootstrap::vk-bootstrap imgui)

