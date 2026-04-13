#ifndef SMARTFS_H
#define SMARTFS_H

#include "smartfs_config.h"
#include "block_device.h"
#include "journal.h"
#include "wear.h"
#include "ml_predict.h"
#include <cstdint>
#include <string>
#include <ctime>

// ============================================================
// SmartFS — Adaptive Embedded Filesystem
// ============================================================

struct SuperBlock
{
    int magic;
    int version;
    int blockSize;
    int totalBlocks;
    int allocMode;
    int writeCount;
};

struct DirEntry
{
    char name[MAX_FILENAME];
    int startBlock;
    int size;
};

class SmartFS
{
public:
    SmartFS();
    ~SmartFS();

    // Core API
    bool format(const std::string &imagePath);
    bool mount(const std::string &imagePath);
    void unmount();

    // File operations
    bool create(const char *name);
    bool write(const char *name, const void *data, int size);
    bool read(const char *name, void *buf, int maxSize, int &bytesRead);
    bool del(const char *name);

    // Maintenance
    void gc();
    void listFiles();
    int  getAllocMode() const { return m_super.allocMode; }
    void resetWorkloadStats() { m_writeCount = 0; m_totalWriteSize = 0; m_startTime = time(nullptr); }
    void setStartTimeOffset(int seconds) { m_startTime = time(nullptr) - seconds; }

    // Accessors for tools
    BlockDevice&  getBlockDevice()  { return m_bd; }
    WearTracker&  getWearTracker()  { return m_wear; }
    const SuperBlock& getSuperBlock() const { return m_super; }

private:
    BlockDevice  m_bd;
    Journal      m_journal;
    WearTracker  m_wear;
    SuperBlock   m_super;
    DirEntry     m_dir[MAX_FILES];
    int          m_fat[TOTAL_BLOCKS];
    uint8_t      m_bitmap[TOTAL_BLOCKS];
    bool         m_mounted;

    // Workload tracking
    int    m_writeCount;
    time_t m_startTime;
    float  m_totalWriteSize;

    // Internal helpers
    void loadSuperBlock();
    void saveSuperBlock();
    void loadBitmap();
    void saveBitmap();
    void loadFAT();
    void saveFAT();
    void loadDir();
    void saveDir();

    int allocateBlock();
    void freeChain(int startBlock);
    int findFile(const char *name);
    void updateAllocMode(int writeSize);
};

#endif // SMARTFS_H
