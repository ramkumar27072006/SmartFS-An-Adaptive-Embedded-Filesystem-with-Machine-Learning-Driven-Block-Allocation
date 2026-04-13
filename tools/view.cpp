#include "smartfs.h"
#include "viewer.h"
#include <iostream>
#include <cstring>

// ============================================================
// view.exe — Block visualization tool
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

    // Verify superblock
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

    Viewer viewer;
    viewer.init(&bd);
    viewer.printMap();

    bd.close();
    return 0;
}
