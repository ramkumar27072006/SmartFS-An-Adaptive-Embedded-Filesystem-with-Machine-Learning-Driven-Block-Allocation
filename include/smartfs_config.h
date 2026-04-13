#ifndef SMARTFS_CONFIG_H
#define SMARTFS_CONFIG_H

// ============================================================
// SmartFS Configuration Constants
// ============================================================

#define BLOCK_SIZE       512
#define TOTAL_BLOCKS     4096

// Layout
#define BLOCK_SUPERBLOCK 0
#define BLOCK_BITMAP     1
#define BLOCK_WEAR       2
#define BLOCK_FAT_START  3
#define BLOCK_FAT_END    34
#define BLOCK_DIR_START  35
#define BLOCK_DIR_END    66
#define BLOCK_DATA_START 67

// Superblock
#define SUPERBLOCK_MAGIC 0x53465331

// Directory
#define MAX_FILES        32
#define MAX_FILENAME     16

// Allocation modes
#define ALLOC_SEQUENTIAL 0
#define ALLOC_RANDOM     1
#define ALLOC_WEAR_AWARE 2

// FAT special values
#define FAT_FREE         0
#define FAT_EOF         -1
#define FAT_RESERVED    -2

// Journal
#define JOURNAL_NONE     0
#define JOURNAL_BEGIN    1
#define JOURNAL_COMMIT   2

#endif // SMARTFS_CONFIG_H
