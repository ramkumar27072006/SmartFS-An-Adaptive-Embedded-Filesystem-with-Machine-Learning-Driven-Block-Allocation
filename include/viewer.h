#ifndef VIEWER_H
#define VIEWER_H

#include "smartfs_config.h"
#include "block_device.h"

// ============================================================
// Block Viewer — prints visual map of disk layout
// ============================================================

class Viewer
{
public:
    Viewer();

    void init(BlockDevice *bd);

    // Print disk map with labels: SUPER, BM, WEAR, FAT, DIR, DATA, FREE
    void printMap();

private:
    BlockDevice *m_bd;
};

#endif // VIEWER_H
