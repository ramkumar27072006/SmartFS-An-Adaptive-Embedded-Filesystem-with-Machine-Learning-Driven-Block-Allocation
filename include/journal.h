#ifndef JOURNAL_H
#define JOURNAL_H

#include "smartfs_config.h"
#include "block_device.h"
#include <cstdint>

// ============================================================
// Write-Ahead Journal for Crash Recovery
// ============================================================

struct JournalEntry
{
    int state;           // JOURNAL_NONE, JOURNAL_BEGIN, JOURNAL_COMMIT
    int targetBlock;     // block being modified
    uint8_t oldData[BLOCK_SIZE]; // backup of original data
};

class Journal
{
public:
    Journal();

    void init(BlockDevice *bd);
    void begin(int targetBlock);
    void commit();
    void recover();

private:
    BlockDevice *m_bd;
    JournalEntry m_entry;

    void saveJournal();
    void loadJournal();
};

#endif // JOURNAL_H
