#include<Window.h>

#include<iostream>

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
        //TODO: Window creation logic
    }

    void Window::destroy()
    {
        //TODO: Destroy logic
    }

    void Window::update()
    {
        //TODO: Update logic
    }

    bool Window::shouldClose() const
    {
        //TODO: Should close logic
    }
}