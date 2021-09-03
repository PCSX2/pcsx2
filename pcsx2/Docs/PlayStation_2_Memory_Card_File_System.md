 PlayStation 2 Memory Card File System 

PlayStation 2 Memory Card File System
=====================================

By [Ross Ridge](https://plus.google.com/101505960668383360394/?rel=author)
--------------------------------------------------------------------------

Public Domain
-------------

This document is a description of the file system layout as used on PlayStation 2 memory cards. It's based on the research I did while writing [mymc](http://www.csclub.uwaterloo.ca/%7Erridge/mymc), a utility for working with PS2 memory card images. This document tries to be comprehensive an accurate, but some details may be missing, misleading or just plain wrong. At lot of assumptions had to be made during my research, and it's hard know to what exactly Sony intended in every case. All most all of the names for structures, fields and flags were made up by me. Nothing in this document should be considered official.

For brevity, unused fields and flag bits are omitted from the tables. In most cases unused fields or flags should be assumed to be either reserved or padding and set to zero when writing. In particular, you'll need to pad out the structures to the length given at the top of the table. All values are stored on the card using the little-endian byte order.

NAND Flash Memory Basics
------------------------

### Glossary

A number of terms are used in this document that you may not familiar with or are different from other sources describing PS2 memory cards, so I've created short glossary.

block

See "erase block".

cluster

The unit of allocation used in the file system. A cluster is one or more pages in size.

ECC

Error correcting code. A means of encoding data so that random bit errors can be detected and corrected.

erase block

The basic erasable unit on a memory card.

`half`

A two byte unsigned half-word value.

page

The basic addressable unit on a memory card. Corresponds to page on the flash device used in the memory card, and is analogous to a sector on hard disk.

programming

The operation of changing erased bits on a flash device from 1 to 0.

superblock

The first page on the memory card where important information about the structure of the file system is kept.

`word`

A four byte unsigned word value.

Since PlayStation 2 (PS2) memory cards use NAND Flash it's helpful to understand some of the basics about this kind of memory works. Flash is a non-volatile form of memory that can be electrically erased and reprogrammed. Since it's non-volatile, meaning it doesn't need power to retain data, and reprogramable, meaning the data stored can be changed, it's the ideal form memory to use in memory card. However, flash memory does have number of limitations which affect how the PS2 uses its memory cards.

One of the limitations is a relatively slow random access time. While much faster than hard disk, NAND flash is much slower than RAM at reading random bytes in memory. Reading the first byte of data from a flash device takes about 25 microseconds. Fortunately, reading on sequentially is much faster. After the first byte is read, each subsequent byte takes only about 50 nanoseconds to read. For this reason NAND flash memory is organized in to pages, similar to how a hard drive is organized into sectors. For example, with the TC58V64AFT, a flash device used in PS2 memory cards, an entire 528 byte page can be read at a sustained 10 Mb/s transfer rate. By comparison, the transfer rate for reading random bytes is much slower, only 40kb/s. _Because the serial bus the PS2 uses to talk to memory cards isn't capable of 10 Mb/s, actual transfer rates will be slower._

The biggest limitation of flash devices is how they're written. Instead changing bits of memory to 0 or 1 depending on the data being written, flash memory can only change a bit from 1 to 0. Once a bit of memory has been changed to 0, it can't be changed back to 1 except by erasing it. Erasing is an operation that sets an entire range of memory, called an erase block, to all 1s. Once a block is erased, it can be written through an operation called programming which changes 1 bits to 0 bits. Since an erase block is made up of a number of pages, this makes writing a more complicated operation than it would be on a hard disk. In order to write a single page to a flash device you need to erase to the block that the page belongs to. However, since that will erase all the pages that belong to that block, not just the page being written, you first need to read in all of the other pages that are going to be erased. After the block is erased you then can program the erase block with the contents of the page you're trying to write and along with the content of all the other pages. _Some flash devices work the other way, erasing sets the block to all 0s while programming changes 0s to 1s._

Another limitation is that flash memory isn't as reliable as RAM memory. A flash device will typically have a certain number of bad blocks when it ships from the factory, and it's possible for defects to appear in the media as it used. Also, a block can eventually "wear out" after a few hundred thousand program/erase cycles. For this reason each page is divided into two parts, a data area a spare area. The data area is used to store ordinary data, while the much smaller spare area is for software defined error-correcting codes (ECC), wear leveling, bad block remapping, and other functions meant to deal with defects in the media.

The flash devices used in PS2 memory cards have a 528 byte page size. This is divided into a 512 byte data area and 16 byte spare area. The spare area is used to store 12 bytes of ECC data, with the remaining 4 bytes left unused. Erase blocks are 16 pages long. The are 16384 pages, for a total combined raw data area capacity 8,388,608 bytes.

File System Organization
------------------------

### Standard Memory  Card Layout

| Address     |    Name             |
|-------------| --------------------|
| `0x0000`    | Superblock          |
| `0x0001`    | Unused              |
| `0x0010`    | Indirect FAT Table  |
| `0x0012`    | FAT Table           |
| `0x0052`    | Allocatable Clusters|
| `0x3ED2`    | Reserved Clusters   |
| `0x3FE0`    | Backup Block 2      |
| `0x3FF0`    | Backup Block 1      |
| `0x3FFF`    | Backup Block 1      |

The PS2 memory card file system has a fairly simple design, with some allowances made for the limitations of flash memory. Its overall structure is similar to the well known MS-DOS FAT file system. It uses a file allocation table (FAT) to keep track of allocated space and a hierarchical directory system where all of a file's metadata is stored in its directory entry. Like the FAT file system, which groups disk sectors into clusters, the PS2 memory card file system groups flash memory pages in to clusters. On standard PS2 memory cards, the cluster size 1024 bytes, or 2 pages long.

### The Superblock

The key to the PS2 memory card file system is the superblock. Located in the first page of the memory, this is the only part of the file system with a fixed location. _While some things like the do end up in fixed locations on standard 8M memory cards, you shouldn't rely on this._

            Superblock (340 bytes)
| Offset |    Name             |   type    |  Default |Description                                                                                     |
| -------|---------------------|-----------|----------|------------------------------------------------------------------------------------------------|
| `0x00` | `magic`             | `byte[28]`| `-`      |Identifies the card as being formatted. Set to the ASCII string "`Sony PS2 Memory Card Format`".|
| `0x1C` | `version`           | `byte[12]`| `1.X.0.0`|Version number of the format used. Version 1.2 indicates full support for `bad_block_list`.     |
| `0x28` | `page len`          | `half`    | `512`    |Size in bytes of a memory card page.                                                            |
| `0x2A` | `pages_per cluster` | `half`    | `2`      |Size in bytes of a memory card page.                                                            |
| `0x2C` | `pages per block`   | `half`    | `16`     |The number of pages in an erase block.                                                          |
| `0x2E` | `-`                 | `half`    | `0xFF00` |Doesn't seem to be used                                                                         |
| `0x30` | `clusters_per_card` | `word`    | `8192`   |The total size of the card in clusters.                                                         |
| `0x34` | `alloc offset`      | `word`    | `41`     |Cluster offset of the first allocatable cluster.                                                |
|        |                     |           |          |Cluster values in the FAT and directory entries are relative to                                 |
|        |                     |           |          |this._This is the cluster immediately after  the FAT                                            |
| `0x38` | `alloc end`         | `word`    | `8135`   |The cluster after the highest allocatable cluster. Relative to `alloc_offset`. _Not used._      |
| `0x3C` | `rootdir_cluster`   | `word`    | `0`      |First cluster of the root directory. Relative to `alloc_offset`.  _Must be zero._               |
| `0x40` | `backup_block1`     | `word`    | `1023`   |Erase block used as a backup area during programming.                                           |
|        |                     |           |          |_Normally the the last block on the card, it may have a different value .                       |
|        |                     |           |          |if that block was found to be bad.                                                              |
| `0x44` | `backup_block2`     | `word`    | `1022`   |This block should be erased to all ones.  _Normally the the second last block on the card._     |
| `0x50` | `ifc_list`          | `word[32]`| `8`      |List of indirect FAT clusters.  _On a standard 8M card there's only one indirect FAT cluster._  |
| `0xD0` | `bad_block_list`    | `word[32]`| `-1`     |List of erase blocks that have errors and shouldn't be used.                                    |
| `0x150`| `card_type`         | `byte`    | `2`      |Memory card type.  _Must be 2, indicating that this is a PS2 memory card._                      |
| `0x151`| `card_flags`        | `byte`    | `0x52`   |Physical characteristics of the memory card.                                                    |

### Card Flags

| Mask      |    Name             | Description                            |
| ----------|---------------------|----------------------------------------|
| `0x01`    | `CF_USE_ECC`        |Card supports ECC.                      |
| `0x08`    | `CF_BAD_BLOCK`      |Card may have bad blocks.               |
| `0x10`    | `CF_ERASE_ZEROES`   |Erased blocks have all bits set to zero.|



Most of the fields in the superblock should be self-explanatory. The fields `page_len`, `pages_per_cluster`, `pages_per_block,` and `clusters_per_card` define the basic geometry of the file system. The FAT can be accessed using `ifc_list` and `rootdir_cluster` gives the first cluster of the root directory. Cluster offsets in FAT and directory entries are all relative to `alloc_offset`

File systems ment to be compatible with the PS2's memory card drivers have a fairly restricted set of geometry options. The field `page_size` can be either 512 or 1024. If the page size is 512 then `pages_per_cluster` can either be 1 or 2, otherwise it can only be 1. The limit on `pages_per_block` is 16. There doesn't seem to be any upper limit on `clusters_per_card`, however because of the size of `ifc_list` there can be no more than 8,388,608 allocatable clusters. While the clusters the make up the FAT can be located anywhere, the value of `rootdir_cluster` must be 0, indicating that the first allocatable cluster is the first cluster of the root directory.

### File Allocation Table

The file allocation table is used for keeping track of which clusters are in use and form a linked-list of the clusters that belong to each file. Each entry in the table is a 32-bit value. If the the cluster corresponding to the entry is free then the most significant bit will be clear. Otherwise, it will be set and the lower 31-bits of the value are the index of the next cluster in the file. These indexes are relative to `alloc_offset` given in the superblock. A value of `0xFFFFFFFF` indicates that this entry is the last cluster in the file.

Unlike the MS-DOS FAT file system, the table isn't required to to be in single contiguous range of clusters. Instead a system of double-indirect indexing is used that allows the clusters that make up the file allocation table to be scattered across file system. The `ifc_list` in the superblock contains a table 32-bit cluster indexes (relative to the start of the card). The entries in the `ifc_list` point to the clusters that make up the indirect table. The indirect table is also a table 32-bit cluster indexes and these indexes point to the clusters that make up the file allocation table.

So assuming a cluster size of 1024, code to access an entry in the FAT might look something like this:

<code>fat_offset = fat_index % 256  
indirect_index = fat_index / 256  
indirect_offset = indirect_index % 256  
dbl_indirect_index = indirect_index / 256  
indirect_cluster_num = superblock.ifc_table[dbl_indirect_index]  
indirect_cluster = read_cluster(indirect_cluster_num)  
fat_cluster_num = indirect_cluster[indirect_offset]  
fat_cluster = read_cluster(fat_cluster_num)  
entry = fat_cluster[fat_offset]`</code>

### Directories

Directories are for the most part like regular files, except that they contain directory entries rather than data. The root directory is as its name suggests, the root of the card's hierarchical directory structure. The first cluster of the root directory is given in the `rootdir_cluster` field of the superblock, and subsequent clusters (if any) can be found by following chain of linked clusters in the FAT.

     Directory Entry Mode Flags 

|    Mask     |   Name       | Description                                                  |
|-------------|---------------|--------------------------------------------------------------|
|`0x0001`     |`DF_READ`      |Read permission                                               |
|`0x0002`     |`DF_WRITE`     |Write permission                                              |
|`0x0004`     |`DF_EXECUTE`   |Execute permission                                            |
|             |               |_Unused_                                                      |
|`0x0008`     |`DF_PROTECTED` |Directory is copy protected.                                  |
|             |               |_Meaningful only to the browser                               |
|`0x0010`     |`DF_FILE`      |Regular file.                                                 |
|`0x0020`     |`DF_DIRECTORY` |Directory                                                     |
|`0x0040`     |`-`            |_Used internally to create directories                        |
|`0x0080`     |`-`            |_Copied?_                                                     |
|`0x0100`     |`-`            |-                                                             |
|`0x0200`     |`O_CREAT`      |_Used to create files._                                       |
|`0x0400`     |`DF_0400`      |RSet when files and directories are created, otherwise ignored|
|`0x0800`     |`DF_POCKETSTN` |PocketStation application save file.                          |
|`0x1000`     |`DF_PSX`       |PlayStation save file.                                        |
|`0x2000`     |`DF_HIDDEN`    |File is hidden.                                               |
|`0x4000`     |`-`            |-                                                             |
|`0x8000`     |`DF_EXISTS`    |This entry is in use. If this flag is clear,                  |
|             |               | then the file or directory has been deleted.                 |

        Directory Entry (512 bytes)

| Offset      |    Name    | Type     | Description                                              |
|-------------|------------|----------|----------------------------------------------------------|
|`0x00`       | `mode`     | `half`   |See directory mode table.                                 |
|`0x04`       | `length`   | `word`   |Length in bytes if a file, or entries if a directory.     |
|`0x08`       | `created`  |`byte[8]` |Creation time.                                            |
|`0x10`       | `cluster`  | `word`   |First cluster of the file,or`0xFFFFFFFF`for an empty file.|
|             |            |          | In "." entries this the first cluster                    |
|             |            |          | of this directory's parent directory instead.            |
|             |            |          | Relative to `alloc_offset`.                              |
|`0x14`       | `dir_entry`| `word`   |Only in "." entries. Entry of this                        |
|             |            |          |directory in its parent's directory.                      |
|`0x18`       | `modified` |`byte[8]` |Modification time.                                        |
|`0x20`       | `attr`     | `word`   |User defined attribute                                    |
|`0x40`       | `name`     |`byte[32]`|Zero terminated name for this directory entry.            |

Zero terminated name for this directory entry.  

The first two directory entries in any directory are always two dummy entries named "." and "..", in that order. In path names, these two directory entries represent the current directory and the parent directory, as they do in Unix, but they serve different purposes in the file system. The first directory entry, the "." entry, is used to store a link to the parent directory. The second entry serves no purpose except as a place holder. The fields these entries do not reflect state of the directories that they are supposed to refer to. The `length` and `cluster` fields are always set 0 and the `modified` time never changes.

The first directory entry in the root directory is special case. It fills the role of the root directory's own directory entry. Unlike the "." entry in other directories, the fields in this entry are used and reflect the state of the root directory. In particular, the `length` field contains the number of directory entries root directory. The exception is the `cluster` field which isn't used.

### Time of Day (8 bytes)


| Offset  | Name  |   type  |Description      |
|---------|-------|---------|-----------------|
|`0x01`   |`sec`  |`byte`   | seconds         |
|`0x02`   |`min`  |`byte`   | minutes         |
|`0x03`   |`hour` |`byte`   | hours           |
|`0x04`   |`days` |`byte`   | day of the month|
|`0x05`   |`month`|`byte`   | month (1-12)    |
|`0x06`   |`year` |`word`   | year            |

The `created` and `modified` fields use the time format given in the Time of Day table. All time stamps use the Japan timezone (+9 UTC), regardless of the timezone the PS2 console was configured to use. A four digit year used.

Most of the mode flags don't serve any purpose in the structure of the file system and only have meaning to higher level software, like the PS2 browser. The `DF_PSX` flag indicates that file was copied from a PSX memory card. If the `DF_POCKETSTN` flag is set as well, the file is a PocketStation application file copied from a PocketStation.

Each directory entry is a massive 512 bytes long, so only two entries fit in each 1024 cluster. The unused bytes at the end of directory could be used for a longer name, but normally names are truncated to only 32 bytes. File names are case sensitive, and the characters "`?`", "`*`", and "`/`" along with all ASCII control characters are illegal in files.

Error Management
----------------

A number of strategies are employed in the file system to handle errors are likely to occur when using memory cards.

### Error Correction Code

The first line defence is the use of error correcting codes to deal with any defects in the card's flash media. The 512 byte data area of each page is divided into 128 byte long chunks and for each chunk a simple 20-bit Hamming code is calculated. This code allows a single bit error in a chunk to be both detected and corrected. A total of three bytes are used to store this 20-bit Hamming code. The first byte contains the column (or bit-wise) parity bits, with the even groups in the lower nibble and the odd groups in the upper nibble. The second and third bytes contain the even and odd groups respectively for the line (or byte-wise) parity bits. The three ECC bytes for each of the four 128-byte chunks of a page are stored in order in the page's spare area.

### Backup Blocks

Two complete erase blocks are reserved to deal with the possibility of the memory card being removed by the user when data is being saved. Since writing a single cluster to card requires erasing and reprogramming the entire erase block the cluster belongs to, a failure during programming could destroy more then just data being written. The two backup blocks are used to ensure the an program operation completes atomically. Either programming completes successfully and no data is lost, or the erase block being programmed is left unchanged and only the new data being written is lost.

Before programming an erase block, both `backup_block1` and `backup_block2` are erase. Then `backup_block1` programmed with a backup copy of the new data for block, and the number of the erase block being programmed is written at the start of the `backup_block2`. The erase block being programmed is then erased and programmed. Finally, `backup_block2` is erased.

Recovery from failed program operation caused by removal of the memory card is implemented whenever a memory card is inserted into the PS2. First `backup_block2` is checked, if it's in a erased state then the last programming operation completed successfully and nothing else is done. If it's not erased, then programming is assumed to have not been completed. The contents of `backup_block1` are then copied to the erase block given in the first word of `backup_block2`. Then `backup_block2` is erased.

### Bad Sector List

The last defence against errors is a list of bad erase blocks kept in the superblock. If any part of an erase block is found to be defective it can be added to `bad_block_list`. No new clusters in this block should be allocated, however clusters already allocated in the block can still continue to be used.

### Reserved Clusters

The standard PS2 memory card drivers artificially reduce the number allocatable clusters by rounding the number down to the nearest 1000s clusters. Since clusters in blocks in the `bad_block_list` don't count as against the limit, this effectively creates a range of reserved replacement clusters. As blocks are marked as bad, clusters in reserved range become available and so the apparent capacity of the memory card remains the same. _This was probably implemented so that memory cards shipped with varying numbers of bad blocks would all appear to have the same amount of free space in the PS2 browser._

See Also
--------

### NAND Flash Memory

Micron: [NAND Flash 101: An Introduction to NAND Flash and How to Design It In to Your Next Product](http://download.micron.com/pdf/technotes/nand/tn2919.pdf)  
Wikipedia: [Flash memory](http://en.wikipedia.org/wiki/Flash_memory)  

### Error Correction Codes

STMicroelectronics: [Error Correction Code in Single Level Cell NAND Flash memories](http://www.st.com/stonline/products/literature/an/10123.htm)  
Micron: [Hamming Codes for NAND Flash Memories](http://download.micron.com/pdf/technotes/nand/tn2908.pdf)  
Hanimar: [Sample code for calculating ECC values in C](http://www.oocities.com/siliconvalley/station/8269/sma02/sma02.html#ECC)