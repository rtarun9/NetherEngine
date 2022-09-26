#include "Pch.hpp"

#include "Core/Application.hpp"
#include "Core/Engine.hpp"

int main()
{
    nether::core::Application application{};
    nether::core::Engine engine{};

    return application.Run(&engine, L"Nether Engine");
}