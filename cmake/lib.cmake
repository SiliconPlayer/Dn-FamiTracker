# cmake/lib.cmake - Build configuration for headless libdnfamitracker and CLI player

if(BUILD_LIB)
    add_library(dnfamitracker STATIC
        Source/dnfamitracker.cpp
        Source/dnfamitracker.h
        Source/DocumentFile.cpp
        Source/DocumentFile.h
        Source/ModuleException.cpp
        Source/ModuleException.h
        Source/FTMDocument.cpp
        Source/FTMDocument.h
        Source/FTMPlayer.cpp
        Source/FTMPlayer.h

        # Data model
        Source/PatternNote.cpp
        Source/PatternData.cpp
        Source/Instrument.cpp
        Source/SeqInstrument.cpp
        Source/Instrument2A03.cpp
        Source/InstrumentFDS.cpp
        Source/InstrumentN163.cpp
        Source/InstrumentS5B.cpp
        Source/InstrumentVRC6.cpp
        Source/InstrumentVRC7.cpp
        Source/InstrumentFactory.cpp
        Source/InstrumentManager.cpp
        Source/Sequence.cpp
        Source/SequenceCollection.cpp
        Source/SequenceManager.cpp
        Source/SequenceParser.cpp
        Source/DSample.cpp
        Source/DSampleManager.cpp
        Source/Groove.cpp
        Source/DetuneTable.cpp
        Source/ChannelMap.cpp
        Source/OldSequence.cpp
        Source/Chunk.cpp

        # Channels and playback handlers
        Source/ChannelHandler.cpp
        Source/ChannelFactory.cpp
        Source/ChannelState.cpp
        Source/InstHandlerDPCM.cpp
        Source/InstHandlerVRC7.cpp
        Source/SeqInstHandler.cpp
        Source/SeqInstHandlerFDS.cpp
        Source/SeqInstHandlerN163.cpp
        Source/SeqInstHandlerS5B.cpp
        Source/SeqInstHandlerSawtooth.cpp
        Source/Channels2A03.cpp
        Source/ChannelsVRC6.cpp
        Source/ChannelsVRC7.cpp
        Source/ChannelsFDS.cpp
        Source/ChannelsMMC5.cpp
        Source/ChannelsN163.cpp
        Source/ChannelsS5B.cpp

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
        target_link_libraries(dnfamitracker-cli PRIVATE m pthread dl)
    endif()
endif()
