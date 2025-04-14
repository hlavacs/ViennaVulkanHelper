set(TARGET engine)

set(SOURCE 
	main.cpp
	helper.cpp
	${PROJECT_SOURCE_DIR}/src/VHInclude.cpp
)

set(HEADERS 
	helper.h
	${INCLUDE}/VHBuffer2.h
	${INCLUDE}/VHCommand2.h
	${INCLUDE}/VHDevice2.h
	${INCLUDE}/VHImage2.h
	${INCLUDE}/VHRender2.h
	${INCLUDE}/VHSync2.h
	${INCLUDE}/VHVulkan2.h
)

add_executable(${TARGET} ${SOURCE} ${HEADERS})

target_compile_features(${TARGET} PUBLIC cxx_std_20)

target_include_directories(${TARGET} PRIVATE stb)
target_include_directories(${TARGET} PRIVATE ${Vulkan_INCLUDE_DIR}/volk)
target_include_directories(${TARGET} PRIVATE ${Vulkan_INCLUDE_DIR}/vma)
target_include_directories(${TARGET} SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${TARGET} PRIVATE SDL3::SDL3 vk-bootstrap::vk-bootstrap imgui)

