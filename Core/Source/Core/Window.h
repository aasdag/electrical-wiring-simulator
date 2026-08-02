#pragma once

#include <cstdint>
#include <string>

namespace Core
{
    struct WindowSpecification
    {
        std::string title;
        std::uint32_t width = 1280;
        std::uint32_t height = 720;
        bool isResizeable = true;
        bool vSync = true;
    };

    class Window
    {
    public:
        Window(const WindowSpecification& specification = WindowSpecification());
        ~Window();

        void create();
        void destroy();
        void beginFrame();
	    void endFrame();

        void update();

        bool shouldClose() const;

    private:
        WindowSpecification m_specification;
    };
}
