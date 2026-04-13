#ifndef WEAR_H
#define WEAR_H

#include "smartfs_config.h"
#include "block_device.h"
#include <cstdint>

// ============================================================
// Wear Tracking Layer
// ============================================================

class WearTracker
{
public:
    WearTracker();

    void init(BlockDevice *bd);
    void load();
    void save();

    void recordWrite(int blockNum);
    int  getWear(int blockNum) const;
    int  getMinWearBlock(int startBlock, const uint8_t *bitmap) const;

    void printStats() const;
    int  getTotalWrites() const;

    const uint16_t* getTable() const { return m_wear; }

private:
    BlockDevice *m_bd;
    uint16_t m_wear[TOTAL_BLOCKS];
};

#endif // WEAR_H
