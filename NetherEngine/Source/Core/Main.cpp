#include "Pch.hpp"

#include "Application.hpp"
#include "Engine.hpp"

// Entry point / Launch pad of the project.
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR cmdLine, _In_ int cmdShow)
{
	nether::core::Application application{};
	nether::core::Engine engine{};

	return application.Run(&engine, hInstance, L"Nether Engine");
}