#pragma once
#include "AsyncFileReader.h"
#include <libchdr/chd.h>

class ChdFileReader : public AsyncFileReader
{
    DeclareNoncopyableObject(ChdFileReader);
public:
    virtual ~ChdFileReader(void) { Close(); };

    static bool CanHandle(const wxString &fileName);
    bool Open(const wxString &fileName) override;

    int ReadSync(void *pBuffer, uint sector, uint count) override;

    void BeginRead(void *pBuffer, uint sector, uint count) override;
    int FinishRead(void) override;
    void CancelRead(void) override {};

    void Close(void) override;
    void SetBlockSize(uint blocksize);
    uint GetBlockSize() const;
    uint GetBlockCount(void) const override;
    ChdFileReader(void);

private:
    chd_file *ChdFile;
    u8 *hunk_buffer;
    u32 sector_size;
    u32 sector_count;
    u32 sectors_per_hunk;
    u32 current_hunk;
    u32 async_read;
};
