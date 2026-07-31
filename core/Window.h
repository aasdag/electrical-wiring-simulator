#pragma once

#include<string>

namespace Core
{
    struct WindowSpecification
    {
        std::string title;
        uint32_t width = 1280;
        uint32_t height = 720;
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

        void update();

        bool shouldClose() const;

    private:
        WindowSpecification m_specification;
    };
}