#pragma once

#include<Layer.h>
#include<Window.h>

#include<string>
#include<memory>
#include<vector>

namespace Core
{
    struct ApplicationSpecification
    {
        std::string name = "Application";
        WindowSpecification windowSpec;
    };

    class Application
    {
    public:
        Application(const ApplicationSpecification& specification = ApplicationSpecification());
		~Application();

	void run();
	void stop();

	template<typename TLayer>
	requires(std::is_base_of_v<Layer, TLayer>)
	void pushLayer()
	{
	    m_layerStack.push_back(std::make_unique<TLayer>());
	}

	static Application& get();
	static float getTime();
    private:
	ApplicationSpecification m_specification;
	std::shared_ptr<Window> m_window;
	bool m_running = false;

	std::vector<std::unique_ptr<Layer>> m_layerStack;
    };
}
