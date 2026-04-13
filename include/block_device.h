#ifndef BLOCK_DEVICE_H
#define BLOCK_DEVICE_H

#include "smartfs_config.h"
#include <cstdint>
#include <string>

// ============================================================
// Block Device Abstraction
// Simulation: backed by sd.img file
// Hardware:   backed by SdFat on Teensy
// ============================================================

class BlockDevice
{
public:
    BlockDevice();
    ~BlockDevice();

    bool init(const std::string &imagePath);
    void close();

    bool readBlock(int blockNum, void *buf);
    bool writeBlock(int blockNum, const void *buf);

    bool isOpen() const;

private:
    std::string m_path;
    bool m_open;
    // Uses fstream internally via cpp
};

#endif // BLOCK_DEVICE_H
