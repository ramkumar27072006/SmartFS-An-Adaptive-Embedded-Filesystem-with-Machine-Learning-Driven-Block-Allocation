#include "journal.h"
#include <cstring>
#include <iostream>

// ============================================================
// Write-Ahead Journal Implementation
// Uses a reserved area at end of image (last 2 blocks)
// Block (TOTAL_BLOCKS-2): journal metadata
// Block (TOTAL_BLOCKS-1): journal data backup
// ============================================================

static const int JOURNAL_META_BLOCK = TOTAL_BLOCKS - 2;
static const int JOURNAL_DATA_BLOCK = TOTAL_BLOCKS - 1;

Journal::Journal()
    : m_bd(nullptr)
{
    std::memset(&m_entry, 0, sizeof(m_entry));
}

void Journal::init(BlockDevice *bd)
{
    m_bd = bd;
    loadJournal();
}

void Journal::begin(int targetBlock)
{
    m_entry.state = JOURNAL_BEGIN;
    m_entry.targetBlock = targetBlock;

    // Backup original block data
    m_bd->readBlock(targetBlock, m_entry.oldData);

    saveJournal();
}

void Journal::commit()
{
    m_entry.state = JOURNAL_COMMIT;
    saveJournal();

    // Clear journal after successful commit
    m_entry.state = JOURNAL_NONE;
    m_entry.targetBlock = 0;
    std::memset(m_entry.oldData, 0, BLOCK_SIZE);
    saveJournal();
}

void Journal::recover()
{
    loadJournal();

    if (m_entry.state == JOURNAL_BEGIN)
    {
        // Incomplete transaction — rollback
        std::cout << "[JOURNAL] Incomplete transaction found, rolling back block "
                  << m_entry.targetBlock << "\n";
        m_bd->writeBlock(m_entry.targetBlock, m_entry.oldData);

        // Clear journal
        m_entry.state = JOURNAL_NONE;
        m_entry.targetBlock = 0;
        std::memset(m_entry.oldData, 0, BLOCK_SIZE);
        saveJournal();

        std::cout << "[JOURNAL] Rollback complete.\n";
    }
    else if (m_entry.state == JOURNAL_COMMIT)
    {
        // Transaction was committed but journal not cleared — just clear
        m_entry.state = JOURNAL_NONE;
        saveJournal();
        std::cout << "[JOURNAL] Committed transaction cleaned up.\n";
    }
    else
    {
        std::cout << "[JOURNAL] No recovery needed.\n";
    }
}

void Journal::saveJournal()
{
    // Write journal metadata (state + targetBlock)
    uint8_t metaBuf[BLOCK_SIZE];
    std::memset(metaBuf, 0, BLOCK_SIZE);
    std::memcpy(metaBuf, &m_entry.state, sizeof(int));
    std::memcpy(metaBuf + sizeof(int), &m_entry.targetBlock, sizeof(int));
    m_bd->writeBlock(JOURNAL_META_BLOCK, metaBuf);

    // Write backup data
    m_bd->writeBlock(JOURNAL_DATA_BLOCK, m_entry.oldData);
}

void Journal::loadJournal()
{
    uint8_t metaBuf[BLOCK_SIZE];
    m_bd->readBlock(JOURNAL_META_BLOCK, metaBuf);

    std::memcpy(&m_entry.state, metaBuf, sizeof(int));
    std::memcpy(&m_entry.targetBlock, metaBuf + sizeof(int), sizeof(int));

    m_bd->readBlock(JOURNAL_DATA_BLOCK, m_entry.oldData);
}
