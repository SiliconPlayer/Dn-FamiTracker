# cmake/lib.cmake - Build configuration for headless libdnfamitracker and CLI player

if(BUILD_LIB)
    add_library(dnfamitracker STATIC
        Source/dnfamitracker.cpp
        Source/dnfamitracker.h
    )

    target_compile_features(dnfamitracker PUBLIC cxx_std_17)

    target_include_directories(dnfamitracker PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source/APU>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source/libsamplerate/include>
    )

    if(NOT MSVC)
        target_compile_options(dnfamitracker PRIVATE
            -fPIC
            -Wall
            -Wextra
            -Wno-unused-parameter
        )
    endif()

    target_link_libraries(dnfamitracker PRIVATE samplerate)
endif()

if(BUILD_CLI)
    add_executable(dnfamitracker-cli
        cli/main.cpp
    )

    target_compile_features(dnfamitracker-cli PRIVATE cxx_std_17)

    target_include_directories(dnfamitracker-cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
    )

    if(TARGET dnfamitracker)
        target_link_libraries(dnfamitracker-cli PRIVATE dnfamitracker)
    endif()

    if(NOT MSVC)
        target_link_libraries(dnfamitracker-cli PRIVATE m pthread)
    endif()
endif()
