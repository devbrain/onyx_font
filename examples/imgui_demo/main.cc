//
// ImGui Font Demo - Main Entry Point
//
// Demonstrates onyx_font library with SDL + OpenGL3 + ImGui
//

#include "demo_app.hh"

#include <imgui.h>
#include <imgui_impl_opengl3.h>

#if defined(IMGUI_DEMO_USE_SDL3)
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_opengl.h>
    #include <imgui_impl_sdl3.h>
    #define SDL_INIT_FLAGS 0
    #define SDL_WINDOW_FLAGS (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)
#else
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_opengl.h>
    #include <imgui_impl_sdl2.h>
    #define SDL_INIT_FLAGS SDL_INIT_VIDEO
    #define SDL_WINDOW_FLAGS (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI)
#endif

#include <cstdio>
#include <cstdlib>

namespace {

// OpenGL version for GLSL
#if defined(__APPLE__)
    constexpr const char* GLSL_VERSION = "#version 150";
    constexpr int GL_MAJOR = 3;
    constexpr int GL_MINOR = 2;
    constexpr bool GL_FORWARD_COMPAT = true;
    constexpr bool GL_CORE_PROFILE = true;
#else
    constexpr const char* GLSL_VERSION = "#version 130";
    constexpr int GL_MAJOR = 3;
    constexpr int GL_MINOR = 0;
    constexpr bool GL_FORWARD_COMPAT = false;
    constexpr bool GL_CORE_PROFILE = false;
#endif

} // anonymous namespace

int main(int /*argc*/, char** /*argv*/) {
    // Initialize SDL
#if defined(IMGUI_DEMO_USE_SDL3)
    if (!SDL_Init(SDL_INIT_FLAGS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
#else
    if (SDL_Init(SDL_INIT_FLAGS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
#endif

    // Configure OpenGL context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, GL_MAJOR);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, GL_MINOR);
    if (GL_CORE_PROFILE) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }
    if (GL_FORWARD_COMPAT) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Create window
#if defined(IMGUI_DEMO_USE_SDL3)
    SDL_Window* window = SDL_CreateWindow(
        "onyx_font ImGui Demo",
        1280, 800,
        SDL_WINDOW_FLAGS
    );
#else
    SDL_Window* window = SDL_CreateWindow(
        "onyx_font ImGui Demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 800,
        SDL_WINDOW_FLAGS
    );
#endif

    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create OpenGL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
#if defined(IMGUI_DEMO_USE_SDL3)
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
#else
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
#endif
    ImGui_ImplOpenGL3_Init(GLSL_VERSION);

    // Create demo application
    imgui_demo::demo_app app;
    app.load_default_fonts();

    // Main loop
    bool running = true;
    while (running) {
        // Poll events
        SDL_Event event;
#if defined(IMGUI_DEMO_USE_SDL3)
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window)) {
                running = false;
            }
        }
#else
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                running = false;
            }
        }
#endif

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
#if defined(IMGUI_DEMO_USE_SDL3)
        ImGui_ImplSDL3_NewFrame();
#else
        ImGui_ImplSDL2_NewFrame();
#endif
        ImGui::NewFrame();

        // Render demo UI
        app.render();

        // Render ImGui
        ImGui::Render();

        int display_w, display_h;
#if defined(IMGUI_DEMO_USE_SDL3)
        SDL_GetWindowSizeInPixels(window, &display_w, &display_h);
#else
        SDL_GL_GetDrawableSize(window, &display_w, &display_h);
#endif

        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
#if defined(IMGUI_DEMO_USE_SDL3)
    ImGui_ImplSDL3_Shutdown();
#else
    ImGui_ImplSDL2_Shutdown();
#endif
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
