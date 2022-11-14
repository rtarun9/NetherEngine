#include "Pch.hpp"

#include "Engine.hpp"

int main()
{
    nether::Engine engine{};

    try
    {
        engine.run();
    }
    catch (const std::exception& exception)
    {
        std::cout << "[Exception Caught] : " << exception.what() << std::endl;
        return -1;
    }

    return 0;
}