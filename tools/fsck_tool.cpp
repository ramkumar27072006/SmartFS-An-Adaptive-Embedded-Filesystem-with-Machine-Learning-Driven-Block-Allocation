#include "smartfs.h"
#include "fsck.h"
#include <iostream>
#include <cstring>

// ============================================================
// fsck.exe — Filesystem consistency checker tool
// ============================================================

static const char *IMAGE = "sd.img";

int main()
{
    BlockDevice bd;
    if (!bd.init(IMAGE))
    {
        std::cerr << "Failed to open " << IMAGE << "\n";
        return 1;
    }

    // Verify it's a SmartFS image
    uint8_t buf[BLOCK_SIZE];
    bd.readBlock(BLOCK_SUPERBLOCK, buf);
    SuperBlock super;
    std::memcpy(&super, buf, sizeof(SuperBlock));

    if (super.magic != SUPERBLOCK_MAGIC)
    {
        std::cerr << "Not a SmartFS image.\n";
        bd.close();
        return 1;
    }

    Fsck checker;
    checker.init(&bd);
    int errors = checker.check();

    bd.close();

    return (errors > 0) ? 1 : 0;
}
