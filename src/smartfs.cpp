#include "smartfs.h"
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <algorithm>

// ============================================================
// SmartFS — Core Filesystem Implementation
// ============================================================

static const char *allocModeName(int mode)
{
    switch (mode)
    {
    case ALLOC_SEQUENTIAL: return "SEQUENTIAL";
    case ALLOC_RANDOM:     return "RANDOM";
    case ALLOC_WEAR_AWARE: return "WEAR-AWARE";
    default:               return "UNKNOWN";
    }
}

SmartFS::SmartFS()
    : m_mounted(false), m_writeCount(0), m_startTime(0), m_totalWriteSize(0)
{
    std::memset(&m_super, 0, sizeof(m_super));
    std::memset(m_dir, 0, sizeof(m_dir));
    std::memset(m_fat, 0, sizeof(m_fat));
    std::memset(m_bitmap, 0, sizeof(m_bitmap));
}

SmartFS::~SmartFS()
{
    if (m_mounted)
        unmount();
}

// ============================================================
// Format — create fresh filesystem
// ============================================================
bool SmartFS::format(const std::string &imagePath)
{
    if (!m_bd.init(imagePath))
    {
        std::cerr << "[SmartFS] Failed to init block device.\n";
        return false;
    }

    // Initialize superblock
    m_super.magic      = SUPERBLOCK_MAGIC;
    m_super.version    = 1;
    m_super.blockSize  = BLOCK_SIZE;
    m_super.totalBlocks = TOTAL_BLOCKS;
    m_super.allocMode  = ALLOC_WEAR_AWARE;
    m_super.writeCount = 0;
    saveSuperBlock();

    // Initialize bitmap — mark metadata blocks as used
    std::memset(m_bitmap, 0, sizeof(m_bitmap));
    for (int i = 0; i <= BLOCK_DIR_END; i++)
        m_bitmap[i] = 1;
    // Reserve journal blocks
    m_bitmap[TOTAL_BLOCKS - 2] = 1;
    m_bitmap[TOTAL_BLOCKS - 1] = 1;
    saveBitmap();

    // Initialize FAT — all free
    for (int i = 0; i < TOTAL_BLOCKS; i++)
        m_fat[i] = FAT_FREE;
    // Mark reserved blocks
    for (int i = 0; i <= BLOCK_DIR_END; i++)
        m_fat[i] = FAT_RESERVED;
    m_fat[TOTAL_BLOCKS - 2] = FAT_RESERVED;
    m_fat[TOTAL_BLOCKS - 1] = FAT_RESERVED;
    saveFAT();

    // Initialize empty directory
    std::memset(m_dir, 0, sizeof(m_dir));
    for (int i = 0; i < MAX_FILES; i++)
        m_dir[i].startBlock = -1;
    saveDir();

    // Initialize wear table
    m_wear.init(&m_bd);
    m_wear.save();

    // Clear journal
    m_journal.init(&m_bd);

    m_bd.close();
    std::cout << "[SmartFS] Filesystem formatted on " << imagePath << "\n";
    return true;
}

// ============================================================
// Mount
// ============================================================
bool SmartFS::mount(const std::string &imagePath)
{
    if (m_mounted)
    {
        std::cerr << "[SmartFS] Already mounted.\n";
        return false;
    }

    if (!m_bd.init(imagePath))
    {
        std::cerr << "[SmartFS] Failed to open block device.\n";
        return false;
    }

    // Load superblock
    loadSuperBlock();
    if (m_super.magic != SUPERBLOCK_MAGIC)
    {
        std::cerr << "[SmartFS] Invalid superblock magic. Not a SmartFS image.\n";
        m_bd.close();
        return false;
    }

    // Load metadata
    loadBitmap();
    loadFAT();
    loadDir();

    // Init wear tracker
    m_wear.init(&m_bd);
    m_wear.load();

    // Init journal and recover if needed
    m_journal.init(&m_bd);
    m_journal.recover();

    // Init workload tracking
    m_writeCount = 0;
    m_totalWriteSize = 0;
    m_startTime = std::time(nullptr);

    m_mounted = true;
    std::cout << "[SmartFS] Mounted. Alloc mode: " << allocModeName(m_super.allocMode) << "\n";
    return true;
}

// ============================================================
// Unmount
// ============================================================
void SmartFS::unmount()
{
    if (!m_mounted)
        return;

    saveSuperBlock();
    saveBitmap();
    saveFAT();
    saveDir();
    m_wear.save();

    m_bd.close();
    m_mounted = false;
    std::cout << "[SmartFS] Unmounted.\n";
}

// ============================================================
// Create file
// ============================================================
bool SmartFS::create(const char *name)
{
    if (!m_mounted)
        return false;

    // Check if file already exists
    if (findFile(name) >= 0)
    {
        std::cerr << "[SmartFS] File '" << name << "' already exists.\n";
        return false;
    }

    // Find empty directory slot
    int slot = -1;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (m_dir[i].startBlock == -1)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        std::cerr << "[SmartFS] Directory full.\n";
        return false;
    }

    std::memset(m_dir[slot].name, 0, MAX_FILENAME);
    std::strncpy(m_dir[slot].name, name, MAX_FILENAME - 1);
    m_dir[slot].startBlock = -1; // No blocks yet
    m_dir[slot].size = 0;

    saveDir();
    return true;
}

// ============================================================
// Write file
// ============================================================
bool SmartFS::write(const char *name, const void *data, int size)
{
    if (!m_mounted || size <= 0)
        return false;

    int slot = findFile(name);
    if (slot < 0)
    {
        // Auto-create
        if (!create(name))
            return false;
        slot = findFile(name);
        if (slot < 0)
        {
            std::cerr << "[SmartFS] Failed to allocate directory entry.\n";
            return false;
        }
    }

    // Free old chain if any
    if (m_dir[slot].startBlock >= 0)
        freeChain(m_dir[slot].startBlock);

    // Calculate blocks needed
    int blocksNeeded = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    const uint8_t *src = reinterpret_cast<const uint8_t *>(data);

    int prevBlock = -1;
    int firstBlock = -1;
    uint8_t buf[BLOCK_SIZE];

    for (int i = 0; i < blocksNeeded; i++)
    {
        // Update ML allocation mode
        updateAllocMode(size);

        int blk = allocateBlock();
        if (blk < 0)
        {
            std::cerr << "[SmartFS] Out of space.\n";
            return false;
        }

        if (firstBlock < 0)
            firstBlock = blk;

        // Journal the data write
        m_journal.begin(blk);

        // Write data
        std::memset(buf, 0, BLOCK_SIZE);
        int chunkSize = std::min(BLOCK_SIZE, size - i * BLOCK_SIZE);
        std::memcpy(buf, src + i * BLOCK_SIZE, chunkSize);
        m_bd.writeBlock(blk, buf);
        m_wear.recordWrite(blk);

        m_journal.commit();

        // Update FAT chain
        m_fat[blk] = FAT_EOF;
        if (prevBlock >= 0)
            m_fat[prevBlock] = blk;
        prevBlock = blk;

        // Mark used
        m_bitmap[blk] = 1;
    }

    // Update directory entry
    m_dir[slot].startBlock = firstBlock;
    m_dir[slot].size = size;

    // Update superblock write count
    m_super.writeCount++;

    // Persist metadata
    saveFAT();
    saveBitmap();
    saveDir();
    saveSuperBlock();
    m_wear.save();

    std::cout << "[SmartFS] Wrote " << size << " bytes to '" << name
              << "' (mode: " << allocModeName(m_super.allocMode) << ")\n";
    return true;
}

// ============================================================
// Read file
// ============================================================
bool SmartFS::read(const char *name, void *buf, int maxSize, int &bytesRead)
{
    if (!m_mounted)
        return false;

    int slot = findFile(name);
    if (slot < 0)
    {
        std::cerr << "[SmartFS] File not found: " << name << "\n";
        return false;
    }

    int fileSize = m_dir[slot].size;
    int toRead = std::min(fileSize, maxSize);
    bytesRead = 0;

    uint8_t *dst = reinterpret_cast<uint8_t *>(buf);
    int blk = m_dir[slot].startBlock;
    uint8_t blockBuf[BLOCK_SIZE];

    while (blk >= 0 && blk != FAT_EOF && bytesRead < toRead)
    {
        m_bd.readBlock(blk, blockBuf);
        int chunkSize = std::min(BLOCK_SIZE, toRead - bytesRead);
        std::memcpy(dst + bytesRead, blockBuf, chunkSize);
        bytesRead += chunkSize;
        blk = m_fat[blk];
    }

    return true;
}

// ============================================================
// Delete file
// ============================================================
bool SmartFS::del(const char *name)
{
    if (!m_mounted)
        return false;

    int slot = findFile(name);
    if (slot < 0)
    {
        std::cerr << "[SmartFS] File not found: " << name << "\n";
        return false;
    }

    // Free block chain
    if (m_dir[slot].startBlock >= 0)
        freeChain(m_dir[slot].startBlock);

    // Clear directory entry
    std::memset(m_dir[slot].name, 0, MAX_FILENAME);
    m_dir[slot].startBlock = -1;
    m_dir[slot].size = 0;

    saveFAT();
    saveBitmap();
    saveDir();

    std::cout << "[SmartFS] Deleted '" << name << "'.\n";
    return true;
}

// ============================================================
// Garbage Collection
// ============================================================
void SmartFS::gc()
{
    if (!m_mounted)
        return;

    int freed = 0;
    // Rebuild bitmap from FAT
    // First, mark all data blocks as free
    for (int i = BLOCK_DATA_START; i < TOTAL_BLOCKS - 2; i++)
    {
        if (m_fat[i] == FAT_FREE && m_bitmap[i] != 0)
        {
            m_bitmap[i] = 0;
            freed++;
        }
    }

    // Then walk all active file chains to mark used blocks
    for (int f = 0; f < MAX_FILES; f++)
    {
        if (m_dir[f].startBlock < 0)
            continue;
        int blk = m_dir[f].startBlock;
        int chainLen = 0;
        while (blk >= 0 && blk != FAT_EOF && chainLen < TOTAL_BLOCKS)
        {
            m_bitmap[blk] = 1;
            blk = m_fat[blk];
            chainLen++;
        }
    }

    saveBitmap();
    if (freed > 0)
        std::cout << "[GC] Reclaimed " << freed << " orphan blocks.\n";
    else
        std::cout << "[GC] No orphan blocks found.\n";
}

// ============================================================
// List files
// ============================================================
void SmartFS::listFiles()
{
    if (!m_mounted)
        return;

    std::cout << "\n--- SmartFS File Listing ---\n";
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (m_dir[i].name[0] != '\0')
        {
            // Count blocks in chain
            int blocks = 0;
            int blk = m_dir[i].startBlock;
            while (blk >= 0 && blk != FAT_EOF && blocks < TOTAL_BLOCKS)
            {
                blocks++;
                blk = m_fat[blk];
            }
            std::cout << "  " << m_dir[i].name
                      << "  size=" << m_dir[i].size
                      << "  blocks=" << blocks
                      << "  start=" << m_dir[i].startBlock << "\n";
            count++;
        }
    }
    if (count == 0)
        std::cout << "  (empty)\n";
    std::cout << "Total files: " << count << "\n";
    std::cout << "Alloc mode:  " << allocModeName(m_super.allocMode) << "\n";
    std::cout << "Write count: " << m_super.writeCount << "\n\n";
}

// ============================================================
// Internal: Load/Save SuperBlock
// ============================================================
void SmartFS::loadSuperBlock()
{
    uint8_t buf[BLOCK_SIZE];
    m_bd.readBlock(BLOCK_SUPERBLOCK, buf);
    std::memcpy(&m_super, buf, sizeof(SuperBlock));
}

void SmartFS::saveSuperBlock()
{
    uint8_t buf[BLOCK_SIZE];
    std::memset(buf, 0, BLOCK_SIZE);
    std::memcpy(buf, &m_super, sizeof(SuperBlock));
    m_bd.writeBlock(BLOCK_SUPERBLOCK, buf);
}

// ============================================================
// Internal: Load/Save Bitmap
// ============================================================
void SmartFS::loadBitmap()
{
    uint8_t buf[BLOCK_SIZE];
    m_bd.readBlock(BLOCK_BITMAP, buf);
    // Bitmap fits in one block for 4096 blocks (4096 bytes > 512)
    // We'll use multiple blocks or pack bits — for simplicity, 1 byte per block
    // Need 4096 bytes = 8 blocks. Use block 1 for first 512 entries.
    // For the full system we store across blocks.

    // Read blocks 1 through 1 + ceil(4096/512) - 1
    int blocksNeeded = (TOTAL_BLOCKS + BLOCK_SIZE - 1) / BLOCK_SIZE; // 8 blocks
    // But we only have 1 block allocated for bitmap in layout
    // Compromise: pack as bits (4096 bits = 512 bytes = 1 block)
    std::memset(m_bitmap, 0, sizeof(m_bitmap));
    m_bd.readBlock(BLOCK_BITMAP, buf);
    for (int i = 0; i < TOTAL_BLOCKS; i++)
    {
        int byteIdx = i / 8;
        int bitIdx = i % 8;
        if (byteIdx < BLOCK_SIZE)
            m_bitmap[i] = (buf[byteIdx] >> bitIdx) & 1;
    }
}

void SmartFS::saveBitmap()
{
    uint8_t buf[BLOCK_SIZE];
    std::memset(buf, 0, BLOCK_SIZE);
    for (int i = 0; i < TOTAL_BLOCKS; i++)
    {
        int byteIdx = i / 8;
        int bitIdx = i % 8;
        if (byteIdx < BLOCK_SIZE && m_bitmap[i])
            buf[byteIdx] |= (1 << bitIdx);
    }
    m_bd.writeBlock(BLOCK_BITMAP, buf);
}

// ============================================================
// Internal: Load/Save FAT
// ============================================================
void SmartFS::loadFAT()
{
    // FAT: 4096 * 4 bytes = 16384 bytes = 32 blocks (blocks 3-34)
    uint8_t buf[BLOCK_SIZE];
    int entriesPerBlock = BLOCK_SIZE / sizeof(int); // 128

    for (int b = BLOCK_FAT_START; b <= BLOCK_FAT_END; b++)
    {
        m_bd.readBlock(b, buf);
        int offset = (b - BLOCK_FAT_START) * entriesPerBlock;
        for (int j = 0; j < entriesPerBlock && (offset + j) < TOTAL_BLOCKS; j++)
        {
            std::memcpy(&m_fat[offset + j], buf + j * sizeof(int), sizeof(int));
        }
    }
}

void SmartFS::saveFAT()
{
    uint8_t buf[BLOCK_SIZE];
    int entriesPerBlock = BLOCK_SIZE / sizeof(int);

    for (int b = BLOCK_FAT_START; b <= BLOCK_FAT_END; b++)
    {
        std::memset(buf, 0, BLOCK_SIZE);
        int offset = (b - BLOCK_FAT_START) * entriesPerBlock;
        for (int j = 0; j < entriesPerBlock && (offset + j) < TOTAL_BLOCKS; j++)
        {
            std::memcpy(buf + j * sizeof(int), &m_fat[offset + j], sizeof(int));
        }
        m_bd.writeBlock(b, buf);
    }
}

// ============================================================
// Internal: Load/Save Directory
// ============================================================
void SmartFS::loadDir()
{
    // Directory: 32 entries * sizeof(DirEntry) bytes
    // DirEntry = 16 + 4 + 4 = 24 bytes
    // 32 * 24 = 768 bytes = 2 blocks (blocks 35-66, use 35-36)
    uint8_t buf[BLOCK_SIZE];
    int entriesPerBlock = BLOCK_SIZE / sizeof(DirEntry);

    for (int i = 0; i < MAX_FILES; i++)
    {
        int blockIdx = BLOCK_DIR_START + i / entriesPerBlock;
        int offsetInBlock = (i % entriesPerBlock) * sizeof(DirEntry);

        if (offsetInBlock == 0)
            m_bd.readBlock(blockIdx, buf);

        std::memcpy(&m_dir[i], buf + offsetInBlock, sizeof(DirEntry));
    }
}

void SmartFS::saveDir()
{
    uint8_t buf[BLOCK_SIZE];
    int entriesPerBlock = BLOCK_SIZE / sizeof(DirEntry);

    int currentBlock = -1;
    for (int i = 0; i < MAX_FILES; i++)
    {
        int blockIdx = BLOCK_DIR_START + i / entriesPerBlock;
        int offsetInBlock = (i % entriesPerBlock) * sizeof(DirEntry);

        if (blockIdx != currentBlock)
        {
            // Save previous block if we were writing one
            if (currentBlock >= 0)
                m_bd.writeBlock(currentBlock, buf);
            std::memset(buf, 0, BLOCK_SIZE);
            currentBlock = blockIdx;
        }

        std::memcpy(buf + offsetInBlock, &m_dir[i], sizeof(DirEntry));
    }
    // Save last block
    if (currentBlock >= 0)
        m_bd.writeBlock(currentBlock, buf);
}

// ============================================================
// Internal: Block Allocation
// ============================================================
int SmartFS::allocateBlock()
{
    switch (m_super.allocMode)
    {
    case ALLOC_SEQUENTIAL:
    {
        for (int i = BLOCK_DATA_START; i < TOTAL_BLOCKS - 2; i++)
        {
            if (m_bitmap[i] == 0)
                return i;
        }
        return -1;
    }

    case ALLOC_RANDOM:
    {
        // Collect free blocks
        int freeBlocks[TOTAL_BLOCKS];
        int freeCount = 0;
        for (int i = BLOCK_DATA_START; i < TOTAL_BLOCKS - 2; i++)
        {
            if (m_bitmap[i] == 0)
                freeBlocks[freeCount++] = i;
        }
        if (freeCount == 0)
            return -1;
        return freeBlocks[std::rand() % freeCount];
    }

    case ALLOC_WEAR_AWARE:
    {
        return m_wear.getMinWearBlock(BLOCK_DATA_START, m_bitmap);
    }

    default:
        return -1;
    }
}

// ============================================================
// Internal: Free a block chain
// ============================================================
void SmartFS::freeChain(int startBlock)
{
    int blk = startBlock;
    int safetyCount = 0;
    while (blk >= 0 && blk != FAT_EOF && safetyCount < TOTAL_BLOCKS)
    {
        int next = m_fat[blk];
        m_fat[blk] = FAT_FREE;
        m_bitmap[blk] = 0;
        blk = next;
        safetyCount++;
    }
}

// ============================================================
// Internal: Find file in directory
// ============================================================
int SmartFS::findFile(const char *name)
{
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (m_dir[i].name[0] != '\0')
        {
            if (std::strncmp(m_dir[i].name, name, MAX_FILENAME) == 0)
                return i;
        }
    }
    return -1;
}

// ============================================================
// Internal: Update allocation mode via ML
// ============================================================
void SmartFS::updateAllocMode(int writeSize)
{
    m_writeCount++;
    m_totalWriteSize += writeSize;

    float avgSize = m_totalWriteSize / m_writeCount;

    time_t now = std::time(nullptr);
    double elapsed = std::difftime(now, m_startTime);
    if (elapsed < 1.0)
        elapsed = 1.0;
    float wps = static_cast<float>(m_writeCount) / static_cast<float>(elapsed);

    int newMode = ml_predict(avgSize, wps);

    if (newMode != m_super.allocMode)
    {
        std::cout << "[ML] Switching allocation: "
                  << allocModeName(m_super.allocMode) << " -> "
                  << allocModeName(newMode)
                  << " (avg_size=" << avgSize << ", wps=" << wps << ")\n";
        m_super.allocMode = newMode;
    }
}
