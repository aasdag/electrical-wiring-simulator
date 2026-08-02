#pragma once

#include "Core/Layer.h"
#include "Core/Window.h"

#include <string>
#include <memory>
#include <vector>
#include <concepts>

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
	requires std::derived_from<TLayer, Layer>
	void pushLayer()
	{
	    m_layerStack.push_back(std::make_unique<TLayer>());
	}

	void queueTransition(Layer* currentLayer, std::unique_ptr<Layer> newLayer);

	static Application& get();
	static float getTime();
    private:
	ApplicationSpecification m_specification;
	std::shared_ptr<Window> m_window;
	bool m_running = false;

	struct LayerTransition
	{
		Layer* currentLayer;
		std::unique_ptr<Layer> newLayer;
	};

	void processLayerTransitions();

	std::vector<std::unique_ptr<Layer>> m_layerStack;
	std::vector<LayerTransition> m_pendingTransitions;

	

    };
}
