#include "Core/Application.h"

int main()
{

	Mupfel::ApplicationSpecification app_spec;
	app_spec.name.insert(0, "My first Application");

	Mupfel::Application& app = Mupfel::Application::Get();

	if (app.Init(app_spec))
	{
		app.Run();
	}

	

	return 0;
}