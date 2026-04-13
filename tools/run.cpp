#include "smartfs.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>

// ============================================================
// run.exe — Normal workload demonstration
// Shows file creation, writing, reading, ML mode switching,
// wear distribution, and crash recovery
// ============================================================

static const char *IMAGE = "sd.img";

int main()
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    SmartFS fs;

    // Step 1: Format
    std::cout << "=== Step 1: Format ===\n";
    if (!fs.format(IMAGE))
        return 1;

    // Step 2: Mount
    std::cout << "\n=== Step 2: Mount ===\n";
    if (!fs.mount(IMAGE))
        return 1;

    // Step 3: Create and write small files (should trigger RANDOM mode)
    std::cout << "\n=== Step 3: Small file workload ===\n";
    char data[256];
    for (int i = 0; i < 20; i++)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "small_%02d.txt", i);
        std::memset(data, 'A' + (i % 26), 256);
        fs.write(name, data, 256);
    }

    fs.listFiles();

    // Step 4: Create and write large files (should trigger SEQUENTIAL mode)
    std::cout << "\n=== Step 4: Large file workload ===\n";
    char bigData[4096];
    for (int i = 0; i < 5; i++)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "big_%02d.dat", i);
        std::memset(bigData, 'X' + (i % 3), 4096);
        fs.write(name, bigData, 4096);
    }

    fs.listFiles();

    // Step 5: Read back a file
    std::cout << "\n=== Step 5: Read back ===\n";
    char readBuf[4096];
    int bytesRead = 0;
    if (fs.read("big_00.dat", readBuf, 4096, bytesRead))
    {
        std::cout << "Read " << bytesRead << " bytes from 'big_00.dat'\n";
        std::cout << "First 16 bytes: ";
        for (int i = 0; i < 16 && i < bytesRead; i++)
            std::cout << readBuf[i];
        std::cout << "\n";
    }

    // Step 6: Delete a file
    std::cout << "\n=== Step 6: Delete ===\n";
    fs.del("small_05.txt");
    fs.listFiles();

    // Step 7: Garbage collection
    std::cout << "\n=== Step 7: GC ===\n";
    fs.gc();

    // Step 8: Wear stats
    std::cout << "\n=== Step 8: Wear Stats ===\n";
    fs.getWearTracker().printStats();

    // Step 9: Unmount
    std::cout << "\n=== Step 9: Unmount ===\n";
    fs.unmount();

    std::cout << "\n=== Workload Demo Complete ===\n";
    return 0;
}
