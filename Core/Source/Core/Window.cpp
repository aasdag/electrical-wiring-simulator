#include <Core/Window.h>

#include <iostream>
#include <raylib.h>

namespace Core
{
    Window::Window(const WindowSpecification& specification)
        :m_specification(specification)
    {

    }

    Window::~Window()
    {
        destroy();
    }

    void Window::create()
    {
        unsigned int flag = 0;

        if(m_specification.isResizeable)
        {
            flag |= FLAG_WINDOW_RESIZABLE;
        }

        SetConfigFlags(flag);

        InitWindow(m_specification.width, m_specification.height, m_specification.title.c_str());
    }

    void Window::destroy()
    {
        if(!IsWindowReady())
        {
            CloseWindow();
        }
    }

    void Window::update()
    {
        //TODO: Update logic
    }

    bool Window::shouldClose() const
    {
        return WindowShouldClose();//TODO: Should close logic
    }
}
