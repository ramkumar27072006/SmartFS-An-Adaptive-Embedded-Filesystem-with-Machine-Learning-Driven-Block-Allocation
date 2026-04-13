#include "smartfs.h"
#include <iostream>
#include <fstream>
#include <cstring>

// ============================================================
// crash.exe — Crash simulation tool
// Writes data then simulates crash (does not commit journal)
// Then remounts to test recovery
// ============================================================

static const char *IMAGE = "sd.img";

int main()
{
    SmartFS fs;

    // Format fresh
    std::cout << "=== Crash Simulation ===\n\n";
    std::cout << "Step 1: Format and create initial data...\n";
    if (!fs.format(IMAGE))
        return 1;
    if (!fs.mount(IMAGE))
        return 1;

    // Write a file
    char data[512];
    std::memset(data, 'A', 512);
    fs.write("important.txt", data, 512);
    fs.listFiles();
    fs.unmount();

    // Simulate crash: write directly to block device without committing journal
    std::cout << "\nStep 2: Simulating crash during write...\n";
    {
        BlockDevice bd;
        bd.init(IMAGE);

        // Start a journal entry (BEGIN state)
        uint8_t metaBuf[BLOCK_SIZE];
        std::memset(metaBuf, 0, BLOCK_SIZE);
        int state = JOURNAL_BEGIN;
        int targetBlock = 67; // First data block
        std::memcpy(metaBuf, &state, sizeof(int));
        std::memcpy(metaBuf + sizeof(int), &targetBlock, sizeof(int));
        bd.writeBlock(TOTAL_BLOCKS - 2, metaBuf); // Journal meta block

        // Backup original data to journal
        uint8_t origData[BLOCK_SIZE];
        bd.readBlock(67, origData);
        bd.writeBlock(TOTAL_BLOCKS - 1, origData); // Journal data block

        // Corrupt the data block (simulating partial write)
        uint8_t corrupt[BLOCK_SIZE];
        std::memset(corrupt, 0xFF, BLOCK_SIZE);
        bd.writeBlock(67, corrupt);

        std::cout << "  Journal: BEGIN (not committed)\n";
        std::cout << "  Block 67: corrupted with 0xFF\n";
        std::cout << "  (Crash! Power lost here)\n";

        bd.close();
    }

    // Remount — should trigger journal recovery
    std::cout << "\nStep 3: Remounting (should recover from crash)...\n";
    SmartFS fs2;
    if (!fs2.mount(IMAGE))
    {
        std::cerr << "Mount failed after crash!\n";
        return 1;
    }

    // Verify file is intact
    char readBuf[512];
    int bytesRead = 0;
    if (fs2.read("important.txt", readBuf, 512, bytesRead))
    {
        std::cout << "\nStep 4: Verify recovered data...\n";
        std::cout << "  Read " << bytesRead << " bytes\n";

        bool intact = true;
        for (int i = 0; i < bytesRead; i++)
        {
            if (readBuf[i] != 'A')
            {
                intact = false;
                break;
            }
        }

        if (intact)
            std::cout << "  Data integrity: PASSED (all bytes are 'A')\n";
        else
            std::cout << "  Data integrity: FAILED (data corrupted)\n";
    }

    fs2.listFiles();
    fs2.unmount();

    std::cout << "\n=== Crash Simulation Complete ===\n";
    return 0;
}
