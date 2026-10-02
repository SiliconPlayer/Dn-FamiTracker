/*
 * dnfamitracker-cli - Command-line test harness & standalone player for Dn-FamiTracker
 */

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#include "miniaudio.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

#ifdef ECHO
static const tcflag_t TERMIOS_ECHO = ECHO;
#undef ECHO
#endif

#include "Source/Common.h"
#include "Source/FTMDocument.h"
#include "Source/FTMPlayer.h"
#include "Source/FamiTrackerTypes.h"

static std::atomic<bool> g_running{true};

static void sigint_handler(int) {
    g_running = false;
}

struct TerminalRawMode {
    struct termios orig;
    bool active = false;

    TerminalRawMode() {
        if (isatty(STDIN_FILENO)) {
            if (tcgetattr(STDIN_FILENO, &orig) == 0) {
                struct termios raw = orig;
                raw.c_lflag &= ~(TERMIOS_ECHO | ICANON);
                raw.c_cc[VMIN] = 0;
                raw.c_cc[VTIME] = 0;
                tcsetattr(STDIN_FILENO, TCSANOW, &raw);
                active = true;
            }
        }
    }

    ~TerminalRawMode() {
        if (active) {
            tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        }
    }
};

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

static void audio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput;
    CFTMPlayer* player = static_cast<CFTMPlayer*>(pDevice->pUserData);
    if (!player) return;
    player->Render(static_cast<float*>(pOutput), static_cast<int>(frameCount));
}

static void PrintUsage(const char* prog) {
    std::cout << "Usage: " << prog << " <module.ftm|module.dnm> [options]\n\n"
              << "Options:\n"
              << "  -p, --play             Realtime interactive audio playback via miniaudio\n"
              << "  -t, --subtune <idx>    Select subtune (1-based index, default: 1)\n"
              << "  -o, --output <out.wav> Render to WAV file\n"
              << "  -s, --seconds <sec>    Render duration limit in seconds (default: full length)\n"
              << "  -m, --mute <ch,...>    Comma-separated list of channel indices to mute\n"
              << "  -i, --info             Print module metadata and exit\n"
              << "  -h, --help             Show this help message\n\n"
              << "Interactive Controls during Playback (-p):\n"
              << "  [Space]        Pause / Resume\n"
              << "  [Left/Right]   Seek -5s / +5s\n"
              << "  [1] - [9]      Toggle mute on channels 1 to 9\n"
              << "  [q] / [Esc]    Quit\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string modulePath;
    std::string wavPath;
    int selectedSubtune = 1;
    double renderSeconds = -1.0;
    bool realtimePlayback = false;
    bool infoOnly = false;
    std::vector<int> channelsToMute;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "-p" || arg == "--play") {
            realtimePlayback = true;
        } else if (arg == "-i" || arg == "--info") {
            infoOnly = true;
        } else if ((arg == "-t" || arg == "--subtune") && i + 1 < argc) {
            selectedSubtune = std::atoi(argv[++i]);
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            wavPath = argv[++i];
        } else if ((arg == "-s" || arg == "--seconds") && i + 1 < argc) {
            renderSeconds = std::atof(argv[++i]);
        } else if ((arg == "-m" || arg == "--mute") && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (!item.empty()) channelsToMute.push_back(std::atoi(item.c_str()));
            }
        } else if (arg[0] != '-' && modulePath.empty()) {
            modulePath = arg;
        } else if (arg[0] != '-' && wavPath.empty()) {
            // Positional fallback: <module> [output.wav] [seconds]
            wavPath = arg;
        } else if (arg[0] != '-' && renderSeconds < 0.0) {
            renderSeconds = std::atof(arg.c_str());
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }

    if (modulePath.empty()) {
        std::cerr << "Error: No module file specified.\n";
        return 1;
    }

    std::cout << "Loading: " << modulePath << "\n";
    CFTMDocument doc;
    if (!doc.LoadDocument(modulePath.c_str())) {
        std::cerr << "Failed to load document: " << modulePath << " (" << (LPCTSTR)doc.GetLastError() << ")\n";
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

    if (infoOnly) {
        return 0;
    }

    std::cout << "\n=== Initializing Audio Player ===\n";
    CFTMPlayer player;
    const int sampleRate = 44100;
    if (!player.AssignDocument(&doc, false)) {
        std::cerr << "Failed to assign document to player!\n";
        return 1;
    }

    int subtuneIdx = selectedSubtune - 1;
    if (subtuneIdx < 0 || subtuneIdx >= (int)doc.GetTrackCount()) {
        std::cerr << "Invalid subtune " << selectedSubtune << ", defaulting to 1.\n";
        subtuneIdx = 0;
    }
    player.SelectSubtune(subtuneIdx);

    for (int ch : channelsToMute) {
        player.SetChannelMuted(ch, true);
    }

    std::cout << "Active channels: " << player.GetChannelCount() << "\n";
    for (int ch = 0; ch < player.GetChannelCount(); ++ch) {
        std::cout << "  Channel " << ch << ": " << player.GetChannelName(ch)
                  << (player.IsChannelMuted(ch) ? " [MUTED]" : "") << "\n";
    }

    double songDuration = player.GetDuration(subtuneIdx);

    // 1. Realtime Interactive Playback Mode (-p)
    if (realtimePlayback) {
        signal(SIGINT, sigint_handler);

        ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
        devConfig.playback.format   = ma_format_f32;
        devConfig.playback.channels = 2;
        devConfig.sampleRate        = sampleRate;
        devConfig.dataCallback      = audio_data_callback;
        devConfig.pUserData         = &player;

        ma_device device;
        if (ma_device_init(nullptr, &devConfig, &device) != MA_SUCCESS) {
            std::cerr << "Failed to initialize audio playback device via miniaudio!\n";
            return 1;
        }

        if (ma_device_start(&device) != MA_SUCCESS) {
            std::cerr << "Failed to start audio playback device!\n";
            ma_device_uninit(&device);
            return 1;
        }

        std::cout << "\n=== Realtime Playback Started (Subsong " << (subtuneIdx + 1) << ") ===\n";
        std::cout << "Controls: [Space] Pause/Resume | [Left/Right] Seek -5s/+5s | [1-9] Mute Channel | [q] Quit\n\n";

        TerminalRawMode rawMode;

        while (g_running && player.IsPlaying() && !player.IsFinished()) {
            char c = 0;
            if (read(STDIN_FILENO, &c, 1) > 0) {
                if (c == 'q' || c == 'Q' || c == 3) {
                    g_running = false;
                    break;
                } else if (c == ' ') {
                    player.SetPaused(!player.IsPaused());
                } else if (c == 27) {
                    char seq[2] = {0, 0};
                    if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                        if (seq[0] == '[') {
                            if (seq[1] == 'C') { // Right Arrow
                                player.Seek(player.GetCurrentTimeSeconds() + 5.0);
                            } else if (seq[1] == 'D') { // Left Arrow
                                double t = player.GetCurrentTimeSeconds() - 5.0;
                                player.Seek(t > 0.0 ? t : 0.0);
                            }
                        }
                    } else {
                        g_running = false;
                        break;
                    }
                } else if (c >= '1' && c <= '9') {
                    int ch = c - '1';
                    if (ch < player.GetChannelCount()) {
                        player.SetChannelMuted(ch, !player.IsChannelMuted(ch));
                    }
                } else if (c == '0') {
                    if (9 < player.GetChannelCount()) {
                        player.SetChannelMuted(9, !player.IsChannelMuted(9));
                    }
                }
            }

            double curSec = player.GetCurrentTimeSeconds();
            int curM = static_cast<int>(curSec) / 60;
            int curS = static_cast<int>(curSec) % 60;
            int totM = static_cast<int>(songDuration) / 60;
            int totS = static_cast<int>(songDuration) % 60;

            std::cout << "\r["
                      << std::setfill('0') << std::setw(2) << curM << ":"
                      << std::setw(2) << curS << " / "
                      << std::setw(2) << totM << ":"
                      << std::setw(2) << totS << "] "
                      << "Frame: " << std::setw(2) << player.GetCurrentFrame()
                      << " Row: " << std::setw(2) << player.GetCurrentRow() << " "
                      << (player.IsPaused() ? "[PAUSED] " : "[PLAYING]")
                      << std::flush;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << "\nStopping playback...\n";
        ma_device_stop(&device);
        ma_device_uninit(&device);
        std::cout << "Done!\n";
        return 0;
    }

    // 2. Offline WAV Export / Synthesis Verification
    double durationToRender = renderSeconds;
    if (durationToRender <= 0.0) {
        if (!wavPath.empty()) {
            durationToRender = (songDuration > 0.0) ? songDuration : 10.0;
        } else {
            durationToRender = 3.0; // Quick 3-second synthesis smoke test
        }
    }

    std::cout << "\n=== Rendering " << durationToRender << "s of Audio ===\n";
    const int chunkSize = 1024;
    std::vector<int16_t> chunkBuffer(chunkSize * 2);
    std::vector<int16_t> allSamples;
    if (!wavPath.empty()) {
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

        if (!wavPath.empty()) {
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

    if (!wavPath.empty()) {
        std::cout << "\nWriting WAV output to: " << wavPath << "\n";
        if (WriteWavFile(wavPath.c_str(), allSamples, sampleRate, 2)) {
            std::cout << "WAV written successfully (" << (allSamples.size() * sizeof(int16_t)) << " bytes)\n";
        } else {
            std::cerr << "Failed to write WAV file!\n";
            return 1;
        }
    }

    std::cout << "\nDone!\n";
    return 0;
}
