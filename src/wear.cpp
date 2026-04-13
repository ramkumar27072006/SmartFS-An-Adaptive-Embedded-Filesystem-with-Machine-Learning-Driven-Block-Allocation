#include "wear.h"
#include <cstring>
#include <iostream>
#include <climits>

// ============================================================
// Wear Tracking Layer Implementation
// ============================================================

WearTracker::WearTracker()
    : m_bd(nullptr)
{
    std::memset(m_wear, 0, sizeof(m_wear));
}

void WearTracker::init(BlockDevice *bd)
{
    m_bd = bd;
}

void WearTracker::load()
{
    // Wear table is stored at BLOCK_WEAR
    // 4096 entries * 2 bytes = 8192 bytes = 16 blocks
    // For simplicity, store first 256 entries in block 2
    // (In full implementation, use multiple blocks)
    uint8_t buf[BLOCK_SIZE];

    // We need 4096 * 2 = 8192 bytes = 16 blocks
    // Use blocks starting from BLOCK_WEAR
    int blocksNeeded = (TOTAL_BLOCKS * sizeof(uint16_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint8_t *ptr = reinterpret_cast<uint8_t *>(m_wear);

    for (int i = 0; i < blocksNeeded && (BLOCK_WEAR + i) < BLOCK_FAT_START; i++)
    {
        m_bd->readBlock(BLOCK_WEAR + i, buf);
        int copySize = BLOCK_SIZE;
        int remaining = TOTAL_BLOCKS * sizeof(uint16_t) - i * BLOCK_SIZE;
        if (remaining < copySize)
            copySize = remaining;
        std::memcpy(ptr + i * BLOCK_SIZE, buf, copySize);
    }
}

void WearTracker::save()
{
    int blocksNeeded = (TOTAL_BLOCKS * sizeof(uint16_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint8_t *ptr = reinterpret_cast<uint8_t *>(m_wear);
    uint8_t buf[BLOCK_SIZE];

    for (int i = 0; i < blocksNeeded && (BLOCK_WEAR + i) < BLOCK_FAT_START; i++)
    {
        std::memset(buf, 0, BLOCK_SIZE);
        int copySize = BLOCK_SIZE;
        int remaining = TOTAL_BLOCKS * sizeof(uint16_t) - i * BLOCK_SIZE;
        if (remaining < copySize)
            copySize = remaining;
        std::memcpy(buf, ptr + i * BLOCK_SIZE, copySize);
        m_bd->writeBlock(BLOCK_WEAR + i, buf);
    }
}

void WearTracker::recordWrite(int blockNum)
{
    if (blockNum >= 0 && blockNum < TOTAL_BLOCKS)
    {
        if (m_wear[blockNum] < UINT16_MAX)
            m_wear[blockNum]++;
    }
}

int WearTracker::getWear(int blockNum) const
{
    if (blockNum >= 0 && blockNum < TOTAL_BLOCKS)
        return m_wear[blockNum];
    return -1;
}

int WearTracker::getMinWearBlock(int startBlock, const uint8_t *bitmap) const
{
    int minWear = INT_MAX;
    int bestBlock = -1;

    for (int i = startBlock; i < TOTAL_BLOCKS - 2; i++) // Reserve last 2 for journal
    {
        if (bitmap[i] == 0) // Free block
        {
            if (m_wear[i] < minWear)
            {
                minWear = m_wear[i];
                bestBlock = i;
            }
        }
    }
    return bestBlock;
}

void WearTracker::printStats() const
{
    int minW = INT_MAX, maxW = 0;
    long totalW = 0;
    int usedBlocks = 0;

    for (int i = BLOCK_DATA_START; i < TOTAL_BLOCKS - 2; i++)
    {
        if (m_wear[i] > 0)
        {
            usedBlocks++;
            totalW += m_wear[i];
            if (m_wear[i] < minW)
                minW = m_wear[i];
            if (m_wear[i] > maxW)
                maxW = m_wear[i];
        }
    }

    if (usedBlocks == 0)
    {
        std::cout << "[WEAR] No writes recorded.\n";
        return;
    }

    std::cout << "[WEAR] Statistics (data blocks):\n";
    std::cout << "  Blocks written: " << usedBlocks << "\n";
    std::cout << "  Min wear:       " << minW << "\n";
    std::cout << "  Max wear:       " << maxW << "\n";
    std::cout << "  Avg wear:       " << (double)totalW / usedBlocks << "\n";
    std::cout << "  Total writes:   " << totalW << "\n";
}

int WearTracker::getTotalWrites() const
{
    long total = 0;
    for (int i = 0; i < TOTAL_BLOCKS; i++)
        total += m_wear[i];
    return (int)total;
}
