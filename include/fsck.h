#ifndef FSCK_H
#define FSCK_H

#include "smartfs_config.h"
#include "block_device.h"

// ============================================================
// Filesystem Consistency Checker
// Detects: orphan blocks, FAT loops, invalid directory entries
// ============================================================

class Fsck
{
public:
    Fsck();

    void init(BlockDevice *bd);

    // Run full consistency check; returns number of errors found
    int check();

private:
    BlockDevice *m_bd;

    int checkOrphanBlocks();
    int checkFATLoops();
    int checkDirectoryEntries();
};

#endif // FSCK_H
