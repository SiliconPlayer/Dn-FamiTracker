# cmake/lib.cmake - Build configuration for headless libdnfamitracker and CLI player

if(BUILD_LIB)
    add_library(dnfamitracker STATIC
        Source/dnfamitracker.cpp
        Source/dnfamitracker.h

        # APU and emulation cores
        Source/Blip_Buffer/Blip_Buffer.cpp
        Source/RegisterState.cpp

        Source/APU/digital-sound-antiques/emu2149.c
        Source/APU/digital-sound-antiques/emu2413.c

        Source/APU/nsfplay/xgm/devices/Sound/nes_apu.cpp
        Source/APU/nsfplay/xgm/devices/Sound/nes_dmc.cpp
        Source/APU/nsfplay/xgm/devices/Sound/nes_mmc5.cpp
        Source/APU/nsfplay/xgm/devices/Sound/nes_vrc6.cpp

        Source/APU/APU.cpp
        Source/APU/SoundChip.cpp
        Source/APU/Mixer.cpp
        Source/APU/2A03.cpp
        Source/APU/VRC6.cpp
        Source/APU/VRC7.cpp
        Source/APU/FDS.cpp
        Source/APU/MMC5.cpp
        Source/APU/N163.cpp
        Source/APU/S5B.cpp
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
