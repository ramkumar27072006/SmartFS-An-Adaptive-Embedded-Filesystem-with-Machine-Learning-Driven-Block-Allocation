#include "smartfs.h"
#include <iostream>
#include <cstring>

// ============================================================
// wear.exe — Wear statistics tool
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

    WearTracker wear;
    wear.init(&bd);
    wear.load();

    std::cout << "\n=== Wear Distribution ===\n";
    wear.printStats();

    // Print top 20 most worn blocks
    std::cout << "\nTop 20 most worn data blocks:\n";

    struct BlockWear
    {
        int block;
        int wear;
    };
    BlockWear top[20];
    for (int i = 0; i < 20; i++)
    {
        top[i].block = -1;
        top[i].wear = -1;
    }

    for (int i = BLOCK_DATA_START; i < TOTAL_BLOCKS - 2; i++)
    {
        int w = wear.getWear(i);
        if (w <= 0)
            continue;

        // Insert into top list if qualifies
        for (int j = 0; j < 20; j++)
        {
            if (w > top[j].wear)
            {
                // Shift down
                for (int k = 19; k > j; k--)
                    top[k] = top[k - 1];
                top[j].block = i;
                top[j].wear = w;
                break;
            }
        }
    }

    for (int i = 0; i < 20; i++)
    {
        if (top[i].block >= 0)
            std::cout << "  Block " << top[i].block << ": " << top[i].wear << " writes\n";
    }

    // Print histogram
    std::cout << "\nWear Histogram (data blocks):\n";
    int buckets[10] = {};
    int maxWear = 0;
    for (int i = BLOCK_DATA_START; i < TOTAL_BLOCKS - 2; i++)
    {
        int w = wear.getWear(i);
        if (w > maxWear)
            maxWear = w;
    }

    if (maxWear > 0)
    {
        int bucketSize = (maxWear + 9) / 10;
        for (int i = BLOCK_DATA_START; i < TOTAL_BLOCKS - 2; i++)
        {
            int w = wear.getWear(i);
            if (w > 0)
            {
                int bucket = (w - 1) / bucketSize;
                if (bucket >= 10)
                    bucket = 9;
                buckets[bucket]++;
            }
        }

        for (int b = 0; b < 10; b++)
        {
            int lo = b * bucketSize + 1;
            int hi = (b + 1) * bucketSize;
            std::cout << "  " << lo << "-" << hi << ": ";
            for (int x = 0; x < buckets[b] && x < 50; x++)
                std::cout << "#";
            std::cout << " (" << buckets[b] << ")\n";
        }
    }
    else
    {
        std::cout << "  (no writes recorded)\n";
    }

    bd.close();
    return 0;
}
