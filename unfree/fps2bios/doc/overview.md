This is a collection of notes which should help anyone looking at the PS2 BIOS. The goal is to fully document the public interface so that a free (GPL) replacement BIOS can be developed for use in emulators such as PCSX2. Where source code examples are given, these refer to the fps2bios source code and not to any original Sony code. The information contained in here has been collected from a number of sources but the main sources are the PCSX2 source code which documents the machine hardware and the open source ps2sdk found at ps2dev.org.

The PS2 BIOS is a collection of files packed into a single file. A file called ROMDIR contains the name and size of each file in the archive. The first file in the archive is called RESET and contains a minimal boot program. The ROMDIR structure is found by looking for the character sequence RESET from the start of the BIOS

# The boot process

The BIOS file is initialized to the memory address 0xBFC00000. The program counter is set to this address and execution started. This is true for both the EE and IOP and so the first few lines of code need to figure out which CPU is currently executing and branch to the relevant initialization code. This code is contained in kernel/start.c. The files kernel/eestart.c and kernel/iopstart.c contain the boot code for the EE and IOP respectively

# The IOP boot process

The IOP boot code is stored in kernel/iopstart.c. This locates the file IOPLOAD in the BIOS image and loads it to the memory address 0x80000000. It then executes the code at 0x80001000. The directory kernel/iopload contains all of the IOP releated BIOS code. Note the linkfile in this directory – this is required to enforce the magic numbers described above. It makes sure that the iopirq.o object is linked at the address 0x80000000 and that iopload is linked starting at 0x80001000.

The boot code found at 0x80001000 is _start() located in iopload.c. This loads the SYSMEM file from the BIOS image and executes its defined entry point. The LOADCORE file is then loaded and its entry point executed. When the LOADCORE entry point returns, the IOP enters an endless loop waiting for an exception.

# IRX linking

The LOADCORE module is responsible mainly for managing the dynamic link relationship between IRX modules. An IRX module is a dynamically linked module – it both exports and imports functions in addition to having an execution entry point. IRX dynamic linking is ordinal based rather than symbolic and, as the public interface is defined by the function export table, this makes figuring it out quite complex. Fortunately, some games have been distributed with debugging symbols and thus this allows us to associate symbolic names with the export ordinals.

The IRX export table is defined as follows:

```
struct irx_export_table {
	u32	magic;
	struct irx_export_table *next;
	u16	version;
	u16	mode;
	u8	name[8];
	void	*fptrs[0];
};
```

Where magic is `0x41c00000` and fptrs is a list of exported functions terminated by a NULL entry.

The IRX import table definition is very similar.

```
struct irx_import_table
{
	u32	magic;
	struct irx_import_table *next;
	u16	version;
	u16	mode;
	char	name[8];
	void	*stubs[0];
}
```

The magic number for the import table is `0x41e00000` and in the same manner as the export table, the stubs list is NULL terminated. An IRX will contain an import table for each module (IRX) that it needs to link with.

To give a concrete example, an IRX that wants to import the `GetLibraryEntryTable` function from the LOADCORE module, could define the import table as follows:

```
loadcore_stub:
	.word	0x41e00000
	.word	0
	.word	0x00000101
	.ascii	"loadcore"
	.align	2

	.globl	GetLibraryEntryTable		# 0x03
GetLibraryEntryTable:
	j	$31
	li	$0, 3

.word	0
	.word	0
```

The label `GetLibraryEntryTable` does not define the linkage itself – this is done by the number (or ordinal) 3. The `li $0, 3` instruction defines that this entry should be linked to the 3rd function exported by the loadcore export table. The function of the LOADCORE module is to resolve the import stub functions with the export tables from the other IRX modules that have been previously registered. It does this by modifying the import table and changing the `j $31` to a `j imm` instruction where imm is the relative address of the exported function.

The LOADCORE code that actually does the linking is in `fix_imports()` and is shown below:

```
int  fix_imports(struct import *imp, struct export *exp){
    func	*ef;
    struct func_stub *fs;
    int  count=0;

    for (ef=exp->func; *ef; ef++){
        count++;		//count number of exported functions
    }
    for (fs=imp->func; fs->jr_ra; fs++) {
        int ordinal = fs->addiu0 & 0xFFFF;
        if (ordinal < count) {
            fs->jr_ra=(((u32)exp->func[ordinal]>>2) & 0x3FFFFFF) | INS_J;
        } else {
            fs->jr_ra=INS_JR_RA;
        }
    }
    imp->flags |=FLAG_IMPORT_QUEUED;
    return 0;
}
```

# Module loading

The first 2 modules loaded are SYSMEM and LOADCORE. After these have been successfully loaded, the remaining modules are loaded in the order specified in iopload.c. The IRX dynamic linking mechanism is very simple and does not account for forward references, so this list must be carefully ordered. Details of the individual modules follow.

# The Exception Manager (EXCEPMAN – version 1.1)

The exception manager is the 3rd module loaded and as the name suggests is responsible for managing the exception handling code. It does not actually implement any of the exception routines but rather provides the plumbing to allow other modules to register exception handlers and to provide for exception handlers to be chained together in priority order.

The Exception Manager maintains an array of 16 pointers (one for each of the 16 possible exceptions) starting at address 0x0440. This address should be an implementation detail but it is used by some modules and so effectively is part of the public interface.

|Ordinal|Function Definition                                                                      |
|-------|-----------------------------------------------------------------------------------------|
|`0x00` |`void _start()`                                                                          |
|`0x01` |`int excepman_reinit()`                                                                  |
|`0x02` |`int excepman_deinit()`                                                                  |
|`0x03` |`void *GetExHandlersTable()`                                                             |
|`0x04` |`int RegisterExceptionHandler(int code, struct exHandler *handler)`                      |
|`0x05` |`int RegisterPriorityExceptionHandler(int code, int priority, struct exHandler *handler)`|
|`0x06` |`int RegisterDefaultExceptionHandler(struct exHandler *handler)`                         |
|`0x07` |`int ReleaseExceptionHandler(int code, struct exHandler *handler)`                       |
|`0x08` |`int ReleaseDefaultExceptionHandler(struct exHandler *handler)`                          |

# The Interrupt Manager (INTRMAN – version 1.2)

The interrupt manager builds upon the services of the exception manager. Its main task is to service the interrupt exception and to call the correct interrupt service for the current interrupt. It installs an exception handler for exception code 0 (interrupt) and exception code 8 (syscall exception). It also deals with context switching of the stack which is used by the thread manager.  
The interrupt table is located directly after the exception table (so starting at 0x0480) – but I don’t think this is important for the public interface.

|Ordinal|Function Definition                                                              |
|-------|---------------------------------------------------------------------------------|
|`0x00` |`void _start()`                                                                  |
|`0x01` |`int intrmanDeinit()`                                                            |
|`0x02` |                                                                                 |
|`0x03` |                                                                                 |
|`0x04` |`int RegisterIntrHandler(int interrupt, int mode, intrh_func handler, void *arg)`|
|`0x05` |`int ReleaseIntrHandler(int interrupt)`                                          |
|`0x06` |`int EnableIntr(int interrupt)`                                                  |
|`0x07` |`int DisableIntr(int interrupt, int *oldstat)`                                   |
|`0x08` |`int CpuDisableIntr()`                                                           |
|`0x09` |`int CpuEnableIntr()`                                                            |
|`0x0A` |`void intrman_syscall_04()`                                                      |
|`0x0B` |`void intrman_syscall_08()`                                                      |
|`0x0C` |`int CpuGetICTRL()`                                                              |
|`0x0D` |`int CpuEnableICTRL()`                                                           |
|`0x0E` |`void intrman_syscall_0C()`                                                      |
|`0x0F` |                                                                                 |
|`0x10` |                                                                                 |
|`0x11` |`int CpuSuspendIntr(u32 *ictrl)`                                                 |
|`0x12` |`int CpuResumeIntr(u32 ictrl)`                                                   |
|`0x13` |`int CpuSuspendIntr(u32 *ictrl)`                                                 |
|`0x14` |`int CpuResumeIntr(u32 ictrl)`                                                   |
|`0x15` |`void intrman_syscall_10()`                                                      |
|`0x16` |`void intrman_syscall_14()`                                                      |
|`0x17` |`int QueryIntrContext()`                                                         |
|`0x18` |`int QueryIntrStack(int stack_pointer)`                                          |
|`0x19` |`int iCatchMultiIntr()`                                                          |
|`0x1A` |`void retonly()`                                                                 |
|`0x1B` |                                                                                 |
|`0x1C` |`void SetCtxSwitchHandler(func handler)`                                         |
|`0x1D` |`func ResetCtxSwitchHandler()`                                                   |
|`0x1E` |`void SetCtxSwitchReqHandler(func handler)`                                      |
|`0x1F` |`func ResetCtxSwitchReqHandler()`                                                |

# Stack frame layout

The interrupt manager also handles the management of the stackframe when context switching between threads. The stack frame is a reserved area on the stack that is large enough to hold the entire processor state. This enables the processor state to be saved and restored between interrupts and context switches. For performance reasons, the entire state is not always stored – an interrupt can define via the mode setting what processor registers it will not preserve and so need saving.  
The stack frame contains a status field which defines how much of it is currently valid so that incremental saving and restoring from it can be achieved. In general, the layout of the stack frame could (and should) be considered as private to the interrupt manager. Given that it is exposed publicly though and the information is available to games, it is documented below.

|Offset|Stored value          |                      |                      |                      |            |
|------|----------------------|----------------------|----------------------|----------------------|------------|
|`0x00`|`FrameID`             |`0xAC0000FE  Mode = 0`|`0xFF00FFFE  Mode = 1`|`0xFFFFFFFE  Mode = 2`|`0xF0FF000C`|
|`0x04`|`AT ($1)`             |x                     |x                     |x                     |            |
|`0x08`|`V0 ($2)`             |x                     |x                     |x                     |x           |
|`0x0C`|`V1 ($3)`             |x                     |x                     |x                     |x           |
|`0x10`|`A0 ($4)`             |x                     |x                     |x                     |x           |
|`0x14`|`A1 ($5)`             |x                     |x                     |x                     |x           |
|`0x18`|`A2 ($6)`             |x                     |x                     |x                     |x           |
|`0x1C`|`A3 ($7)`             |x                     |x                     |x                     |x           |
|`0x20`|`T0 ($8)`             |                      |x                     |x                     |            |
|`0x24`|`T1 ($9)`             |                      |x                     |x                     |            |
|`0x28`|`T2 ($10)`            |                      |x                     |x                     |            |
|`0x2C`|`T3 ($11)`            |                      |x                     |x                     |            |
|`0x30`|`T4 ($12)`            |                      |x                     |x                     |            |
|`0x34`|`T5 ($13)`            |                      |x                     |x                     |            |
|`0x38`|`T6 ($14)`            |                      |x                     |x                     |            |
|`0x3C`|`T7 ($15)`            |                      |x                     |x                     |            |
|`0x40`|`S0 ($16)`            |                      |                      |x                     |            |
|`0x44`|`S1 ($17)`            |                      |                      |x                     |x           |
|`0x48`|`S2 ($18)`            |                      |                      |x                     |x           |
|`0x4C`|`S3 ($19)`            |                      |                      |x                     |x           |
|`0x50`|`S4 ($20)`            |                      |                      |x                     |x           |
|`0x54`|`S5 ($21)`            |                      |                      |x                     |x           |
|`0x58`|`S6 ($22)`            |                      |                      |x                     |x           |
|`0x5C`|`S7 ($23)`            |                      |                      |x                     |x           |
|`0x60`|`T8 ($24)`            |                      |x                     |x                     |            |
|`0x64`|`T9 ($25)`            |                      |x                     |x                     |            |
|`0x68`|`Unused (k0)`         |                      |                      |                      |            |
|`0x6C`|`Unused (k1)`         |                      |                      |                      |            |
|`0x70`|`GP`                  |                      |x                     |x                     |            |
|`0x74`|`SP`                  |x                     |x                     |x                     |x           |
|`0x78`|`FP`                  |                      |x                     |x                     |x           |
|`0x7C`|`RA`                  |x                     |x                     |x                     |x           |
|`0x80`|`HI`                  |x                     |x                     |x                     |            |
|`0x84`|`LO`                  |x                     |x                     |x                     |            |
|`0x88`|`COP0 STATUS`         |x                     |x                     |x                     |x           |
|`0x8C`|`EPC before exception`|x                     |x                     |x                     |x           |
