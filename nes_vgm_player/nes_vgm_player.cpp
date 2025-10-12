#include <array>
#include <vector>
#include <memory>
#include <iostream>
#include <random>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "apu2A03.h"

using namespace std;

#ifdef _WIN32
#define NOMINMAX   // Optional: Avoid min/max macro clashes
#define byte _windows_byte_defined // Prevent 'byte' from being defined in Windows headers
#include <windows.h>
    #include <conio.h>
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <dirent.h>

    // Enable raw mode and nonblocking input
    void enable_raw_mode(void) {
        struct termios term;
        tcgetattr(STDIN_FILENO, &term);
        term.c_lflag &= ~(ICANON | ECHO);  // no buffering, no echo
        tcsetattr(STDIN_FILENO, TCSANOW, &term);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);  // nonblocking read
    }

    void disable_raw_mode(void) {
        struct termios term;
        tcgetattr(STDIN_FILENO, &term);
        term.c_lflag |= (ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &term);
        fcntl(STDIN_FILENO, F_SETFL, 0);
    }
#endif
// ---------------------------------------------------------------------
static SDL_AudioStream *stream = NULL;

bool initSdl()
{
    if (SDL_Init(SDL_INIT_AUDIO) == false)
    {
        SDL_Log("SDL could not initialize! SDL error: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec spec;
    spec.channels = 1;
    spec.format = SDL_AUDIO_U8;
    spec.freq = 44100;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        return false;
    }

    SDL_ResumeAudioStreamDevice(stream);
    return true;
}

bool closeSdl()
{
    SDL_Quit();
    return true;
}

void putAudioStreamData(const void* buf, int len)
{
    if (stream) {
        if (!SDL_PutAudioStreamData(stream, buf, len)) {
            SDL_Log("Couldn't put audio data into stream: %s", SDL_GetError());
        }
        SDL_FlushAudioStream(stream);
    }
}

// ---------------------------------------------------------------------
class VgmPlayer {
public:
    enum class Status { IDLE, FINISHED, PLAYING, QUIT, ST_ERROR, NEXT, PREV };
    VgmPlayer() = default;
    bool load(const std::string& path);
    Status play(Apu2A03& apu);

private:
    std::vector<uint8_t> data;
    size_t dataOffset = 0;
};

bool VgmPlayer::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "Failed to open VGM file: " << path << "\n";
        return false;
    }

    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);

    data.resize(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    if (!f) {
        std::cerr << "Failed to read VGM file\n";
        return false;
    }

    // Check VGM signature
    if (size < 0x40 || std::string(reinterpret_cast<char*>(&data[0]), 4) != "Vgm ") {
        std::cerr << "Invalid VGM header\n";
        return false;
    }

    // Data offset (relative to 0x34 + value)
    uint32_t dataOffsetField = *reinterpret_cast<uint32_t*>(&data[0x34]);
    dataOffset = dataOffsetField ? (0x34 + dataOffsetField) : 0x40;

    if (dataOffset >= size) {
        std::cerr << "Invalid data offset\n";
        return false;
    }

    return true;
}

VgmPlayer::Status VgmPlayer::play(Apu2A03& apu) {
    if (data.empty()) {
        std::cerr << "No VGM data loaded\n";
        return Status::ST_ERROR;
    }

    size_t pos = dataOffset;
    const size_t end = data.size();
    const double samplesPerCpuCycle = (1789773.0 / 44100.0/2); // ≈0.0246 cycles/sample
    const int minimum_audio = 16384;
    while (pos < end) {
        while (SDL_GetAudioStreamQueued(stream) < minimum_audio) {
            uint8_t cmd = data[pos++];

            switch (cmd) {
            case 0x66: // End of sound data
                std::cout << "End of VGM stream\n";
                return Status::FINISHED;

            case 0xB4: { // NES APU write
                if (pos + 2 > end) return Status::ST_ERROR;
                uint8_t addr = data[pos++];
                uint8_t val = data[pos++];
                apu.cpuWrite(0x4000 + addr, val);
                break;
            }

            case 0x61: { // wait n samples
                if (pos + 2 > end) return Status::ST_ERROR;
                uint16_t n = data[pos] | (data[pos + 1] << 8);
                pos += 2;
                uint32_t cycles = static_cast<uint32_t>(n * samplesPerCpuCycle);
                apu.clock(cycles);
                break;
            }

            case 0x62: { // wait 735 samples (60 Hz)
                apu.clock(static_cast<uint32_t>(735 * samplesPerCpuCycle));
                break;
            }

            case 0x63: { // wait 882 samples (50 Hz)
                apu.clock(static_cast<uint32_t>(882 * samplesPerCpuCycle));
                break;
            }

            case 0x67: // Data block
                if (pos + 3 > end) return Status::ST_ERROR;
                {
                    uint8_t extra_cmd = data[pos++];
                    uint8_t type = data[pos++];
                    uint32_t size = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24);
                    pos += 4;
                    if (pos + size > end) return Status::ST_ERROR;
                    // Skip data block for now
                    pos += size;
                }
                break;
            default:
                if (cmd >= 0x70 && cmd <= 0x7F) {
                    // wait (n+1) samples
                    uint8_t n = (cmd & 0x0F) + 1;
                    uint32_t cycles = static_cast<uint32_t>(n * samplesPerCpuCycle);
                    apu.clock(cycles);
                } else {
                    // Unhandled command, skip or stop
                    std::cerr << "Unknown VGM command: 0x" 
                            << std::hex << (int)cmd << std::dec << "\n";
                    return Status::ST_ERROR;
                }
                break;
            }
        }

#ifdef _WIN32
        int ch = -1;
        if (_kbhit()) ch = _getch();
#else
        int ch = getchar();
#endif

        if (ch == 27 || ch == 'q' || ch == 'Q') { // ESC key
            return Status::QUIT;
        } else if (ch == 'n' || ch == 'N')  {
            return Status::NEXT;
        } else if (ch == 'p' || ch == 'P') {
            return Status::PREV;
        } else if (ch != -1) {
            printf("Key pressed: %d\n", ch);
        }
        SDL_Delay(1);
    }
    return Status::FINISHED;
}

Apu2A03 apu;
Bus bus;
Cpu6502 cpu;

void apuInit()
{
    apu.connectBus(&bus);
    apu.connectCPU(&cpu);

    array<uint8_t, 20> initialRegisters = {
        0x30, 0x08, 0x00, 0x00, // Pulse 1
        0x30, 0x08, 0x00, 0x00, // Pulse 2
        0x80, 0x00, 0x00, 0x00, // Triangle
        0x30, 0x00, 0x00, 0x00, // Noise
        0x00, 0x00, 0x00, 0x00, // DMC
    };
    for (size_t i = 0; i < initialRegisters.size(); i++) {
        apu.cpuWrite(0x4000 + i, initialRegisters[i]);
    }
    // Enable all channels
    apu.cpuWrite(0x4015, 0x0F);
    apu.cpuWrite(0x4017, 0x40);
}

int main(int argc, char* argv[])
{
#ifndef _WIN32
    enable_raw_mode();
#endif
    initSdl();
    apuInit();

    VgmPlayer vgm;
    string media_folder = "../../../../";

    vector<string> files;
    // find .vgm files in the current directory
    #ifdef _WIN32
        WIN32_FIND_DATA findFileData;
        HANDLE hFind = FindFirstFile((media_folder + "*.vgm").c_str(), &findFileData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    files.emplace_back(findFileData.cFileName);
                }
            } while (FindNextFile(hFind, &findFileData) != 0);
            FindClose(hFind);
        }
    #else
        //DIR* dir = opendir(".");
        DIR* dir = opendir(media_folder.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_type == DT_REG && strstr(entry->d_name, ".vgm")) {
                    files.emplace_back(entry->d_name);
                }
            }
            closedir(dir);
        } else {
            std::cerr << "Failed to open current directory.\n";
        }
    #endif

    auto it = files.begin();
    while (it != files.end()) {
        string file = media_folder + *it;
        std::cout << "Playing file: " << file << "\n";
        if (!vgm.load(file)) {
            std::cerr << "Failed to load VGM file: " << file << "\n";
            continue;
        }
        auto status = vgm.play(apu);
        if (status == VgmPlayer::Status::QUIT) {
            break;
        }
        else if (status == VgmPlayer::Status::ST_ERROR) {
            std::cerr << "Error during playback of file: " << file << "\n";
        }
        else if (status == VgmPlayer::Status::NEXT) {
            // Go to next file if possible
            auto new_it = std::find(files.begin(), files.end(), *it);
            if (new_it != files.end() && (new_it + 1) != files.end()) {
                ++new_it;
            }
            it = new_it;
        }
        else if (status == VgmPlayer::Status::PREV) {
            // Go back to previous file if possible
            auto new_it = std::find(files.begin(), files.end(), *it);
            if (new_it != files.begin() && new_it != files.end()) {
                --new_it;
            }
            it = new_it;
        }
        else if (status == VgmPlayer::Status::FINISHED) {
            it++;
        }
    }

#ifndef _WIN32
    disable_raw_mode();
#endif
    closeSdl();
    return 0;
}
