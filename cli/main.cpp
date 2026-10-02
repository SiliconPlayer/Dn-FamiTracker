/*
 * dnfamitracker-cli - Command-line test harness for Dn-FamiTracker
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <cmath>
#include "Source/Common.h"
#include "Source/FTMDocument.h"
#include "Source/FTMPlayer.h"
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

static bool WriteWavFile(const char* filename, const std::vector<int16_t>& samples, int sampleRate, int channels) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;

    uint32_t dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    uint32_t fileSize = 36 + dataSize;
    uint32_t byteRate = static_cast<uint32_t>(sampleRate * channels * sizeof(int16_t));
    uint16_t blockAlign = static_cast<uint16_t>(channels * sizeof(int16_t));
    uint16_t bitsPerSample = 16;
    uint16_t audioFormat = 1; // PCM
    uint32_t fmtSize = 16;
    uint16_t numChannels = static_cast<uint16_t>(channels);

    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&fileSize), 4);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    out.write(reinterpret_cast<const char*>(&fmtSize), 4);
    out.write(reinterpret_cast<const char*>(&audioFormat), 2);
    out.write(reinterpret_cast<const char*>(&numChannels), 2);
    out.write(reinterpret_cast<const char*>(&sampleRate), 4);
    out.write(reinterpret_cast<const char*>(&byteRate), 4);
    out.write(reinterpret_cast<const char*>(&blockAlign), 2);
    out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&dataSize), 4);
    out.write(reinterpret_cast<const char*>(samples.data()), dataSize);

    return out.good();
}

int main(int argc, char* argv[]) {
    std::cout << "dnfamitracker-cli - Headless FamiTracker Harness\n";
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <module.ftm|module.dnm> [output.wav] [seconds]\n";
        return 1;
    }

    const char* path = argv[1];
    const char* wavPath = (argc >= 3) ? argv[2] : nullptr;
    double renderSeconds = (argc >= 4) ? std::atof(argv[3]) : 0.0;

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

    std::cout << "\n=== Initializing Audio Player ===\n";
    CFTMPlayer player;
    const int sampleRate = 44100;
    if (!player.AssignDocument(&doc, false)) {
        std::cerr << "Failed to assign document to player!\n";
        return 1;
    }

    std::cout << "Active channels: " << player.GetChannelCount() << "\n";
    for (int ch = 0; ch < player.GetChannelCount(); ++ch) {
        std::cout << "  Channel " << ch << ": " << player.GetChannelName(ch) << "\n";
    }


    // Determine how many seconds to render
    double durationToRender = renderSeconds;
    if (durationToRender <= 0.0) {
        if (wavPath) {
            durationToRender = player.GetDuration(0);
            if (durationToRender <= 0.0) durationToRender = 10.0;
        } else {
            durationToRender = 3.0; // Quick 3-second synthesis smoke test
        }
    }

    std::cout << "\n=== Rendering " << durationToRender << "s of Audio ===\n";
    const int chunkSize = 1024;
    std::vector<int16_t> chunkBuffer(chunkSize * 2);
    std::vector<int16_t> allSamples;
    if (wavPath) {
        allSamples.reserve(static_cast<size_t>(durationToRender * sampleRate * 2));
    }

    int totalFramesRendered = 0;
    int targetFrames = static_cast<int>(durationToRender * sampleRate);
    int16_t peakAmplitude = 0;
    int64_t sumSquares = 0;
    int64_t totalSampleCount = 0;

    while (totalFramesRendered < targetFrames && player.IsPlaying()) {
        int framesToRender = std::min(chunkSize, targetFrames - totalFramesRendered);
        int rendered = player.Render(chunkBuffer.data(), framesToRender);
        if (rendered <= 0) break;

        for (int i = 0; i < rendered * 2; ++i) {
            int16_t sample = chunkBuffer[i];
            int16_t absSample = (sample < 0) ? -sample : sample;
            if (absSample > peakAmplitude) peakAmplitude = absSample;
            sumSquares += static_cast<int64_t>(sample) * sample;
            totalSampleCount++;
        }

        if (wavPath) {
            allSamples.insert(allSamples.end(), chunkBuffer.begin(), chunkBuffer.begin() + rendered * 2);
        }
        totalFramesRendered += rendered;
    }

    double rms = (totalSampleCount > 0) ? std::sqrt(static_cast<double>(sumSquares) / totalSampleCount) : 0.0;
    double peakDb = (peakAmplitude > 0) ? 20.0 * std::log10(static_cast<double>(peakAmplitude) / 32768.0) : -96.0;

    std::cout << "Rendered frames: " << totalFramesRendered << " (" << (static_cast<double>(totalFramesRendered) / sampleRate) << "s)\n";
    std::cout << "Peak Amplitude:  " << peakAmplitude << " / 32768 (" << std::fixed << std::setprecision(1) << peakDb << " dBFS)\n";
    std::cout << "RMS Level:       " << std::fixed << std::setprecision(1) << rms << "\n";

    if (peakAmplitude > 0) {
        std::cout << "Synthesis check: PASS (Audio signal generated successfully)\n";
    } else {
        std::cout << "Synthesis check: WARNING (Output is silent)\n";
    }

    if (wavPath) {
        std::cout << "\nWriting WAV output to: " << wavPath << "\n";
        if (WriteWavFile(wavPath, allSamples, sampleRate, 2)) {
            std::cout << "WAV written successfully (" << (allSamples.size() * sizeof(int16_t)) << " bytes)\n";
        } else {
            std::cerr << "Failed to write WAV file!\n";
            return 1;
        }
    }

    std::cout << "\nDone!\n";
    return 0;
}
