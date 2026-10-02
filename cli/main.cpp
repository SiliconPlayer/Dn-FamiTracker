/*
 * dnfamitracker-cli - Command-line test harness for Dn-FamiTracker
 */

#include <iostream>
#include <iomanip>
#include "Source/Common.h"
#include "Source/FTMDocument.h"
#include "Source/FamiTrackerTypes.h"

static const char* ChipName(unsigned char chip) {
    switch (chip) {
        case SNDCHIP_NONE: return "None (2A03/2A07 only)";
        case SNDCHIP_VRC6: return "Konami VRC6";
        case SNDCHIP_VRC7: return "Konami VRC7";
        case SNDCHIP_FDS:  return "Nintendo FDS";
        case SNDCHIP_MMC5: return "Nintendo MMC5";
        case SNDCHIP_N163: return "Namco 163";
        case SNDCHIP_S5B:  return "Sunsoft 5B";
        default:           return "Unknown";
    }
}

int main(int argc, char* argv[]) {
    std::cout << "dnfamitracker-cli - Headless FamiTracker Harness\n";
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <module.ftm|module.dnm>\n";
        return 1;
    }

    const char* path = argv[1];
    std::cout << "Loading: " << path << "\n";

    CFTMDocument doc;
    if (!doc.LoadDocument(path)) {
        std::cerr << "Failed to load document: " << path << " (" << (LPCTSTR)doc.GetLastError() << ")\n";
        return 1;
    }

    std::cout << "\n=== Module Info ===\n";
    std::cout << "Type:       " << (doc.IsDnModule() ? "Dn-FamiTracker Module (.dnm)" : "FamiTracker Module (.ftm)") << "\n";
    std::cout << "Version:    0x" << std::hex << doc.GetFileVersion() << std::dec << "\n";
    std::cout << "Title:      " << doc.GetSongName() << "\n";
    std::cout << "Artist:     " << doc.GetSongArtist() << "\n";
    std::cout << "Copyright:  " << doc.GetSongCopyright() << "\n";
    std::cout << "Machine:    " << (doc.GetMachine() == NTSC ? "NTSC (60 Hz)" : "PAL (50 Hz)") << "\n";
    std::cout << "Engine Hz:  " << doc.GetFrameRate() << " Hz\n";
    std::cout << "Expansion:  " << ChipName(doc.GetExpansionChip()) << "\n";
    std::cout << "Channels:   " << doc.GetChannelCount() << "\n";
    std::cout << "Subsongs:   " << doc.GetTrackCount() << "\n";

    for (unsigned int i = 0; i < doc.GetTrackCount(); ++i) {
        int frames = doc.GetFrameCount(i);
        int patternLen = doc.GetPatternLength(i);
        int speed = doc.GetSongSpeed(i);
        int tempo = doc.GetSongTempo(i);
        double durationSec = doc.GetStandardLength(i, 0);

        int minutes = static_cast<int>(durationSec) / 60;
        int seconds = static_cast<int>(durationSec) % 60;
        int millis = static_cast<int>((durationSec - static_cast<int>(durationSec)) * 1000);

        std::cout << "\n--- Subsong " << (i + 1) << " / " << doc.GetTrackCount() << " ---\n";
        std::cout << "  Frames:     " << frames << "\n";
        std::cout << "  PatternLen: " << patternLen << "\n";
        std::cout << "  Speed:      " << speed << "\n";
        std::cout << "  Tempo:      " << tempo << "\n";
        std::cout << "  Duration:   " << minutes << ":"
                  << std::setfill('0') << std::setw(2) << seconds << "."
                  << std::setw(3) << millis << " (" << durationSec << "s)\n";
    }

    std::cout << "\nModule loaded successfully!\n";
    return 0;
}
