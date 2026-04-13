#include "viewer.h"
#include "smartfs.h"
#include <cstring>
#include <iostream>
#include <iomanip>

// ============================================================
// Block Viewer — Visual disk map
// ============================================================

Viewer::Viewer()
    : m_bd(nullptr)
{
}

void Viewer::init(BlockDevice *bd)
{
    m_bd = bd;
}

void Viewer::printMap()
{
    if (!m_bd || !m_bd->isOpen())
    {
        std::cerr << "[VIEWER] Block device not open.\n";
        return;
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

    std::cout << "\n=== SmartFS Block Map ===\n";
    std::cout << "Total blocks: " << TOTAL_BLOCKS << "\n\n";

    int blocksPerRow = 16;
    for (int i = 0; i < TOTAL_BLOCKS; i++)
    {
        if (i % blocksPerRow == 0)
            std::cout << std::setw(5) << i << ": ";

        if (i == BLOCK_SUPERBLOCK)
            std::cout << "[SUPER]";
        else if (i == BLOCK_BITMAP)
            std::cout << "[BM   ]";
        else if (i == BLOCK_WEAR)
            std::cout << "[WEAR ]";
        else if (i >= BLOCK_FAT_START && i <= BLOCK_FAT_END)
            std::cout << "[FAT  ]";
        else if (i >= BLOCK_DIR_START && i <= BLOCK_DIR_END)
            std::cout << "[DIR  ]";
        else if (i == TOTAL_BLOCKS - 2 || i == TOTAL_BLOCKS - 1)
            std::cout << "[JRNL ]";
        else if (bitmap[i])
            std::cout << "[DATA ]";
        else
            std::cout << "[FREE ]";

        if ((i + 1) % blocksPerRow == 0)
            std::cout << "\n";
    }

    // Count free data blocks
    int freeData = 0;
    int usedData = 0;
    for (int i = BLOCK_DATA_START; i < TOTAL_BLOCKS - 2; i++)
    {
        if (bitmap[i])
            usedData++;
        else
            freeData++;
    }

    std::cout << "\nData blocks used: " << usedData << "\n";
    std::cout << "Data blocks free: " << freeData << "\n";
    std::cout << "=========================\n\n";
}
