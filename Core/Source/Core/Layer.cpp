#include "Core/Layer.h"

#include "Core/Application.h"

namespace Core {

	void Layer::queueTransition(std::unique_ptr<Layer> toLayer)
	{
        Application::get().queueTransition(this, std::move(toLayer));
	}

}