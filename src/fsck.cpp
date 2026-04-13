#include "fsck.h"
#include "smartfs.h"
#include <cstring>
#include <iostream>

// ============================================================
// Filesystem Consistency Checker
// ============================================================

Fsck::Fsck()
    : m_bd(nullptr)
{
}

void Fsck::init(BlockDevice *bd)
{
    m_bd = bd;
}

int Fsck::check()
{
    std::cout << "\n=== FSCK: Filesystem Consistency Check ===\n";
    int errors = 0;

    errors += checkOrphanBlocks();
    errors += checkFATLoops();
    errors += checkDirectoryEntries();

    if (errors == 0)
        std::cout << "[FSCK] Filesystem is consistent.\n";
    else
        std::cout << "[FSCK] Found " << errors << " error(s).\n";

    std::cout << "=== FSCK Complete ===\n\n";
    return errors;
}

// ============================================================
// Check for orphan blocks:
// Blocks marked used in bitmap but not in any file chain
// ============================================================
int Fsck::checkOrphanBlocks()
{
    // Load metadata
    SuperBlock super;
    uint8_t buf[BLOCK_SIZE];
    m_bd->readBlock(BLOCK_SUPERBLOCK, buf);
    std::memcpy(&super, buf, sizeof(SuperBlock));

    if (super.magic != SUPERBLOCK_MAGIC)
    {
        std::cout << "[FSCK] ERROR: Invalid superblock magic!\n";
        return 1;
    }

    // Load bitmap
    uint8_t bitmapBuf[BLOCK_SIZE];
    uint8_t bitmap[TOTAL_BLOCKS];
    std::memset(bitmap, 0, sizeof(bitmap));
    m_bd->readBlock(BLOCK_BITMAP, bitmapBuf);
    for (int i = 0; i < TOTAL_BLOCKS; i++)
    {
        int byteIdx = i / 8;
        int bitIdx = i % 8;
        if (byteIdx < BLOCK_SIZE)
            bitmap[i] = (bitmapBuf[byteIdx] >> bitIdx) & 1;
    }

    // Load FAT
    int fat[TOTAL_BLOCKS];
    int entriesPerBlock = BLOCK_SIZE / sizeof(int);
    for (int b = BLOCK_FAT_START; b <= BLOCK_FAT_END; b++)
    {
        m_bd->readBlock(b, buf);
        int offset = (b - BLOCK_FAT_START) * entriesPerBlock;
        for (int j = 0; j < entriesPerBlock && (offset + j) < TOTAL_BLOCKS; j++)
            std::memcpy(&fat[offset + j], buf + j * sizeof(int), sizeof(int));
    }

    // Load directory
    DirEntry dir[MAX_FILES];
    int dirEntriesPerBlock = BLOCK_SIZE / sizeof(DirEntry);
    for (int i = 0; i < MAX_FILES; i++)
    {
        int blockIdx = BLOCK_DIR_START + i / dirEntriesPerBlock;
        int offsetInBlock = (i % dirEntriesPerBlock) * sizeof(DirEntry);
        if (offsetInBlock == 0)
            m_bd->readBlock(blockIdx, buf);
        std::memcpy(&dir[i], buf + offsetInBlock, sizeof(DirEntry));
    }

    // Build reachable set from directory chains
    uint8_t reachable[TOTAL_BLOCKS];
    std::memset(reachable, 0, sizeof(reachable));

    // Mark metadata blocks as reachable
    for (int i = 0; i <= BLOCK_DIR_END; i++)
        reachable[i] = 1;
    reachable[TOTAL_BLOCKS - 2] = 1;
    reachable[TOTAL_BLOCKS - 1] = 1;

    // Walk file chains
    for (int f = 0; f < MAX_FILES; f++)
    {
        if (dir[f].startBlock < 0)
            continue;
        int blk = dir[f].startBlock;
        int chainLen = 0;
        while (blk >= 0 && blk != FAT_EOF && chainLen < TOTAL_BLOCKS)
        {
            reachable[blk] = 1;
            blk = fat[blk];
            chainLen++;
        }
    }

    // Check for orphans: used in bitmap but not reachable
    int orphans = 0;
    for (int i = BLOCK_DATA_START; i < TOTAL_BLOCKS - 2; i++)
    {
        if (bitmap[i] && !reachable[i])
        {
            std::cout << "[FSCK] Orphan block: " << i << "\n";
            orphans++;
        }
    }

    if (orphans > 0)
        std::cout << "[FSCK] Total orphan blocks: " << orphans << "\n";
    else
        std::cout << "[FSCK] No orphan blocks.\n";

    return orphans;
}

// ============================================================
// Check for FAT loops
// ============================================================
int Fsck::checkFATLoops()
{
    uint8_t buf[BLOCK_SIZE];

    // Load FAT
    int fat[TOTAL_BLOCKS];
    int entriesPerBlock = BLOCK_SIZE / sizeof(int);
    for (int b = BLOCK_FAT_START; b <= BLOCK_FAT_END; b++)
    {
        m_bd->readBlock(b, buf);
        int offset = (b - BLOCK_FAT_START) * entriesPerBlock;
        for (int j = 0; j < entriesPerBlock && (offset + j) < TOTAL_BLOCKS; j++)
            std::memcpy(&fat[offset + j], buf + j * sizeof(int), sizeof(int));
    }

    // Load directory
    DirEntry dir[MAX_FILES];
    int dirEntriesPerBlock = BLOCK_SIZE / sizeof(DirEntry);
    for (int i = 0; i < MAX_FILES; i++)
    {
        int blockIdx = BLOCK_DIR_START + i / dirEntriesPerBlock;
        int offsetInBlock = (i % dirEntriesPerBlock) * sizeof(DirEntry);
        if (offsetInBlock == 0)
            m_bd->readBlock(blockIdx, buf);
        std::memcpy(&dir[i], buf + offsetInBlock, sizeof(DirEntry));
    }

    int loopErrors = 0;

    for (int f = 0; f < MAX_FILES; f++)
    {
        if (dir[f].startBlock < 0)
            continue;

        // Floyd's cycle detection
        int slow = dir[f].startBlock;
        int fast = dir[f].startBlock;
        bool hasLoop = false;

        while (true)
        {
            // Move slow by 1
            if (slow < 0 || slow == FAT_EOF)
                break;
            slow = fat[slow];

            // Move fast by 2
            if (fast < 0 || fast == FAT_EOF)
                break;
            fast = fat[fast];
            if (fast < 0 || fast == FAT_EOF)
                break;
            fast = fat[fast];

            if (slow == fast)
            {
                hasLoop = true;
                break;
            }
        }

        if (hasLoop)
        {
            std::cout << "[FSCK] FAT loop detected in file '" << dir[f].name << "'\n";
            loopErrors++;
        }
    }

    if (loopErrors == 0)
        std::cout << "[FSCK] No FAT loops.\n";

    return loopErrors;
}

// ============================================================
// Check directory entries
// ============================================================
int Fsck::checkDirectoryEntries()
{
    uint8_t buf[BLOCK_SIZE];

    // Load directory
    DirEntry dir[MAX_FILES];
    int dirEntriesPerBlock = BLOCK_SIZE / sizeof(DirEntry);
    for (int i = 0; i < MAX_FILES; i++)
    {
        int blockIdx = BLOCK_DIR_START + i / dirEntriesPerBlock;
        int offsetInBlock = (i % dirEntriesPerBlock) * sizeof(DirEntry);
        if (offsetInBlock == 0)
            m_bd->readBlock(blockIdx, buf);
        std::memcpy(&dir[i], buf + offsetInBlock, sizeof(DirEntry));
    }

    int errors = 0;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (dir[i].startBlock < 0 && dir[i].size == 0)
            continue; // Empty slot

        // Check name is null-terminated
        bool hasNull = false;
        for (int c = 0; c < MAX_FILENAME; c++)
        {
            if (dir[i].name[c] == '\0')
            {
                hasNull = true;
                break;
            }
        }
        if (!hasNull)
        {
            std::cout << "[FSCK] Dir entry " << i << " name not null-terminated.\n";
            errors++;
        }

        // Check start block in valid range
        if (dir[i].startBlock >= 0 &&
            (dir[i].startBlock < BLOCK_DATA_START || dir[i].startBlock >= TOTAL_BLOCKS - 2))
        {
            std::cout << "[FSCK] Dir entry '" << dir[i].name
                      << "' has invalid start block: " << dir[i].startBlock << "\n";
            errors++;
        }

        // Check size non-negative
        if (dir[i].size < 0)
        {
            std::cout << "[FSCK] Dir entry '" << dir[i].name
                      << "' has negative size: " << dir[i].size << "\n";
            errors++;
        }
    }

    if (errors == 0)
        std::cout << "[FSCK] Directory entries valid.\n";

    return errors;
}
