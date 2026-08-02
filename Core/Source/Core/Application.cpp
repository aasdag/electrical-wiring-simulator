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
            float timeStep = currentTime - lastTime;
            lastTime = currentTime;

            //Main layer update here
            for (const std::unique_ptr<Layer>& layer : m_layerStack)
				layer->onUpdate(/*timestep*/0.001f);

            m_window->beginFrame();
            ClearBackground(RAYWHITE);

			// NOTE: rendering can be done elsewhere (eg. render thread)
			for (const std::unique_ptr<Layer>& layer : m_layerStack)
				layer->onRender();

            m_window->update();

            m_window->endFrame();

            processLayerTransitions();
        }
    }

    void Application::stop()
    {
        m_running = false;
    }

    void Application::queueTransition(Layer* currentLayer, std::unique_ptr<Layer> newLayer)
    {
        m_pendingTransitions.push_back({currentLayer, std::move(newLayer)});
    }


    Application& Application::get()
    {
        return *s_application;
    }

	float Application::getTime()
    {
        return static_cast<float>(GetTime());
    }

    void Application::processLayerTransitions()
    {
        for(auto& transition : m_pendingTransitions)
        {
            for(auto& layer : m_layerStack)
            {
                if(layer.get() == transition.currentLayer)
                {
                    layer = std::move(transition.newLayer);
                    break;
                }
            }
        }

        m_pendingTransitions.clear();
    }
 }
