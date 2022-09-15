set(SRC_FILES
    "Source/Main.cpp")

add_executable(CrystalEngine WIN32 ${SRC_FILES})

target_precompile_headers(CrystalEngine PUBLIC Source/Pch.hpp)