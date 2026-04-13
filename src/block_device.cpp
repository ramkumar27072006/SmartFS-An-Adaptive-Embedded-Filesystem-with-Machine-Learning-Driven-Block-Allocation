#include "block_device.h"
#include <fstream>
#include <cstring>
#include <iostream>

// ============================================================
// Block Device — file-backed SD card simulator
// ============================================================

BlockDevice::BlockDevice()
    : m_open(false)
{
}

BlockDevice::~BlockDevice()
{
    close();
}

bool BlockDevice::init(const std::string &imagePath)
{
    m_path = imagePath;

    // Create image file if it doesn't exist
    std::ifstream test(m_path, std::ios::binary);
    if (!test.good())
    {
        std::ofstream create(m_path, std::ios::binary);
        if (!create.good())
        {
            std::cerr << "[BD] Failed to create image: " << m_path << "\n";
            return false;
        }
        // Pre-allocate entire image (TOTAL_BLOCKS * BLOCK_SIZE)
        char zero[BLOCK_SIZE];
        std::memset(zero, 0, BLOCK_SIZE);
        for (int i = 0; i < TOTAL_BLOCKS; i++)
        {
            create.write(zero, BLOCK_SIZE);
        }
        create.close();
    }
    else
    {
        test.close();
    }

    m_open = true;
    return true;
}

void BlockDevice::close()
{
    m_open = false;
}

bool BlockDevice::readBlock(int blockNum, void *buf)
{
    if (!m_open || blockNum < 0 || blockNum >= TOTAL_BLOCKS)
        return false;

    std::ifstream file(m_path, std::ios::binary);
    if (!file.good())
        return false;

    file.seekg(static_cast<std::streamoff>(blockNum) * BLOCK_SIZE);
    file.read(reinterpret_cast<char *>(buf), BLOCK_SIZE);
    return file.good();
}

bool BlockDevice::writeBlock(int blockNum, const void *buf)
{
    if (!m_open || blockNum < 0 || blockNum >= TOTAL_BLOCKS)
        return false;

    std::fstream file(m_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.good())
        return false;

    file.seekp(static_cast<std::streamoff>(blockNum) * BLOCK_SIZE);
    file.write(reinterpret_cast<const char *>(buf), BLOCK_SIZE);
    return file.good();
}

bool BlockDevice::isOpen() const
{
    return m_open;
}
