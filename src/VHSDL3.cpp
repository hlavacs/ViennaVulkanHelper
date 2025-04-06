#include "VHInclude.h"
#include "VHSDL3.h"


bool SDL3Init( std::string name, int width, int height) {
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
