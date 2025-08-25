cmake_minimum_required(VERSION 3.20)
project(my_clion_project)

set(CMAKE_CXX_STANDARD 17)

# Fix for Cereal debug symbol issues on MinGW
if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wa,-mbig-obj")
    # Alternative: reduce debug info to avoid linker issues
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g1")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g1")
endif()

# --- Find Package Configurations ---
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2-2.32.2/x86_64-w64-mingw32/lib/cmake/SDL2")
find_package(SDL2 REQUIRED)

set(SDL2_ttf_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2_ttf-2.24.0/x86_64-w64-mingw32/lib/cmake/SDL2_ttf")
find_package(SDL2_ttf REQUIRED)

set(SDL2_image_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2_image-2.8.2/x86_64-w64-mingw32/lib/cmake/SDL2_image")
find_package(SDL2_image REQUIRED)

# --- Project Executable ---
add_executable(my_clion_project
        main.cpp
        Analyzers.cpp
        CerealRegistrations.cpp
        Circuit.cpp
        Element.cpp
        ErrorManager.cpp
        GUI.cpp
        GraphExtractor.cpp
        InputParser.cpp
        Menu.cpp
        Node.cpp
        Pin.cpp
        Plotter.cpp
        PlotCursor.cpp
        Probe.cpp
        ProbeManager.cpp
        ProjectSerializer.cpp
        SignalProcessor.cpp
        Solvers.cpp
        TcpSocket.cpp
        Wire.cpp
)

# Add test executables
add_executable(test_simulator test_simulator.cpp)
target_link_libraries(test_simulator ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES})
target_include_directories(test_simulator PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})

# Set C++ standard for test
set_target_properties(test_simulator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add pin wiring test executable
add_executable(test_pin_wiring test_pin_wiring.cpp)
target_link_libraries(test_pin_wiring ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES})
target_include_directories(test_pin_wiring PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})

# Set C++ standard for pin wiring test
set_target_properties(test_pin_wiring PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add MNA system test executable
add_executable(test_mna_system test_mna_system.cpp)
target_link_libraries(test_mna_system ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES})
target_include_directories(test_mna_system PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})

# Set C++ standard for MNA system test
set_target_properties(test_mna_system PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# --- Include Directories ---
target_include_directories(my_clion_project PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/cereal-1.3.2/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2-2.32.2/x86_64-w64-mingw32/include/SDL2"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2_ttf-2.24.0/x86_64-w64-mingw32/include/SDL2"
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2_image-2.8.2/x86_64-w64-mingw32/include/SDL2"
)

# --- Link Libraries ---
target_link_libraries(my_clion_project PRIVATE
        SDL2::SDL2main
        SDL2::SDL2
        SDL2_ttf::SDL2_ttf
        SDL2_image::SDL2_image
        ws2_32
)

# --- Auto-copy DLLs and Assets ---
add_custom_command(TARGET my_clion_project POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2-2.32.2/x86_64-w64-mingw32/bin"
        "$<TARGET_FILE_DIR:my_clion_project>"
        COMMENT "Copying SDL2 DLLs..."
)
add_custom_command(TARGET my_clion_project POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2_ttf-2.24.0/x86_64-w64-mingw32/bin"
        "$<TARGET_FILE_DIR:my_clion_project>"
        COMMENT "Copying SDL2_ttf DLL..."
)
add_custom_command(TARGET my_clion_project POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2_image-2.8.2/x86_64-w64-mingw32/bin"
        "$<TARGET_FILE_DIR:my_clion_project>"
        COMMENT "Copying SDL2_image DLL..."
)
add_custom_command(TARGET my_clion_project POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:my_clion_project>/assets"
        COMMENT "Copying assets folder to build directory..."
)
