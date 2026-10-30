// Per-frame guest cost of the cartridge on the PRG32-QT core (ESP32-C6 model).
//   perf file.prg32 warmup frames
// Mean and peak instructions per frame, with no input: after the idle time the
// title screen starts its demo, so the window covers real play by the autoplayer.
// Adapted from tools/qt-perf/perf.cpp of Galleria2007 (PRG32 family, MIT).
#include "Cartridge.h"
#include "Runtime.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 4) return 2;
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), {});
    std::string e;
    auto c = prg32::Cartridge::parse(b, e);
    if (!c) { std::cerr << e << "\n"; return 3; }
    prg32::Runtime r;
    if (!r.load(*c, e) || !r.init(e)) { std::cerr << e << "\n"; return 4; }
    int warm = std::stoi(argv[2]), frames = std::stoi(argv[3]);
    uint64_t total = 0, peak = 0;
    for (int i = 0; i < warm + frames; i++) {
        r.setPerformanceMode(prg32::PerformanceMode::Esp32C6Accurate);
        uint64_t i0 = r.retiredInstructions();
        if (!r.frame(0, e)) { std::cerr << "frame " << i << ": " << e << "\n"; return 5; }
        if (i >= warm) {
            uint64_t n = r.retiredInstructions() - i0;
            total += n;
            if (n > peak) peak = n;
        }
    }
    // 160 MHz; 1.95 cycles per instruction is the ratio Galleria2007 measured for
    // the model's cost classes. It is a proxy for comparisons, not a promise.
    double mean = double(total) / frames;
    std::cout << "instr/frame mean=" << uint64_t(mean) << " peak=" << peak
              << " modelled_ms mean=" << mean * 1.95 / 160000.0 << " peak=" << double(peak) * 1.95 / 160000.0 << "\n";
    return 0;
}
