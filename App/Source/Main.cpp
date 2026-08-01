#include "Core/Application.h"

#include "EditorLayer.h"

int main()
{
    Core::ApplicationSpecification specification;

    specification.name = "Electrical wiring simulator";
    specification.windowSpec.title = specification.name;
    specification.windowSpec.height = 720;
    specification.windowSpec.width = 1280;
    specification.windowSpec.isResizeable = true;
    specification.windowSpec.vSync = true;

    Core::Application application(specification);
    
    application.pushLayer<EditorLayer>();
    application.run();

    return 0;
}