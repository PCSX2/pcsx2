
void pgifInit(void);

extern void psxGPUw(int, u32);
extern u32 psxGPUr(int);

extern void PGIFw(int, u32);
extern u32 PGIFr(int);

extern void PGIFwQword(u32 addr, void*);
extern void PGIFrQword(u32 addr, void*);

extern u32 psxDma2GpuR(u32 addr);
extern void psxDma2GpuW(u32 addr, u32 data);


extern void ps12PostOut(u32 mem, u8 value);
extern void psDuartW(u32 mem, u8 value);
extern u8 psExp2R8(u32 mem);
extern void kernelTTYFileDescrWrite(u32 mem, u32 data);
extern u32 getIntTmrKReg(u32 mem, u32 data);
extern void testInt(void);
extern void HPCoS_print(u32 mem, u32 data);
extern void anyIopLS(u32 addr, u32 data, int Wr);
extern void dma6_OTClear(void);