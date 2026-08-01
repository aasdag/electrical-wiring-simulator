#include "Core/Application.h"

#include <raylib.h>
#include <imgui.h>
#include <rlImGui.h>

 namespace Core
 {
    static Application* s_application = nullptr;

    Application::Application(const ApplicationSpecification& specification)
        : m_specification(specification)
    {
        s_application = this;

        //TODO: Init application logic

        if(m_specification.windowSpec.title.empty())
        {
            m_specification.windowSpec.title = m_specification.name;
        }

        m_window = std::make_shared<Window>(m_specification.windowSpec);
        m_window->create();

    }

    Application::~Application()
    {
        m_layerStack.clear();

        rlImGuiShutdown();

        m_window.reset();
        
        s_application = nullptr;
    }

    void Application::run()
    {
        m_running = true;

        float lastTime = getTime();

        while(m_running)
        {
            if(m_window->shouldClose())
            {
                stop();
                break;
            }

            float currentTime = getTime();
            //TODO: Implement when math is added: float timeStep = glm::clamp(currentTime - lastTime, 0.001f, 0.1f);
            lastTime = currentTime;

            //Main layer update here
            for (const std::unique_ptr<Layer>& layer : m_layerStack)
				layer->onUpdate(/*timestep*/0.001f);

			// NOTE: rendering can be done elsewhere (eg. render thread)
			for (const std::unique_ptr<Layer>& layer : m_layerStack)
				layer->onRender();

            m_window->update();
        }
    }

    void Application::stop()
    {
        m_running = false;
    }

    Application& Application::get()
    {
        return *s_application;
    }

	float Application::getTime()
    {
        return static_cast<float>(GetTime());
    }
 }
