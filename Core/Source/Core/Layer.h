#pragma once

#include "Core/Event.h"

#include <memory>
#include <utility>

namespace Core
{
    class Layer
    {
    public:
        virtual ~Layer() = default;

        virtual void onEvent(Event& event) {}
        virtual void onUpdate(float ts) {}
        virtual void onRender() {}
        virtual void onImGuiRender() {}

        template<std::derived_from<Layer> T, typename... Args>
        void transitionTo(Args&&... args)
        {
            queueTransition(std::make_unique<T>(std::forward<Args>(args)...));
        }
    private:
        void queueTransition(std::unique_ptr<Layer> layer);
    };
}
