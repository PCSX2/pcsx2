// iVUzerorec.cpp assembly routines
// zerofrog(@gmail.com)

#ifdef __APPLE__
        #define svudispfntemp         _svudispfntemp
        #define s_TotalVUCycles       _s_TotalVUCycles
        #define s_callstack           _s_callstack
        #define s_vu1ebp              _s_vu1ebp
        #define s_vu1esp              _s_vu1esp
        #define s_vu1esi              _s_vu1esi
        #define s_vuedi               _s_vuedi
        #define s_vuebx               _s_vuebx
        #define s_saveebx             _s_saveebx
        #define s_saveecx             _s_saveecx
        #define s_saveedx             _s_saveedx
        #define s_saveesi             _s_saveesi
        #define s_saveedi             _s_saveedi
        #define s_saveebp             _s_saveebp
        #define s_writeQ              _s_writeQ
        #define s_writeP              _s_writeP
        #define g_curdebugvu          _g_curdebugvu
        #define SuperVUGetProgram     _SuperVUGetProgram
        #define SuperVUCleanupProgram _SuperVUCleanupProgram
        #define SuperVUExecuteProgram _SuperVUExecuteProgram
        #define SuperVUEndProgram     _SuperVUEndProgram
        #define g_sseVUMXCSR          _g_sseVUMXCSR
        #define g_sseMXCSR            _g_sseMXCSR
#endif

        
.intel_syntax noprefix
.extern s_TotalVUCycles
.extern s_callstack
.extern s_vu1esp
.extern s_writeQ
.extern s_writeP
.extern g_curdebugvu
.extern SuperVUGetProgram
.extern SuperVUCleanupProgram
.extern g_sseVUMXCSR
.extern g_sseMXCSR
        
// SuperVUExecuteProgram(u32 startpc, int vuindex)
.globl SuperVUExecuteProgram
SuperVUExecuteProgram:
        mov eax, [esp]
        mov dword ptr s_TotalVUCycles, 0
        add esp, 4
        mov dword ptr [s_callstack], eax
        call SuperVUGetProgram
        mov s_vu1esi, esi
        mov s_vuedi, edi
        mov s_vuebx, ebx

        mov s_vu1esp, esp
        and esp, -16		// align stack for GCC compilance

        ldmxcsr g_sseVUMXCSR
        mov dword ptr s_writeQ, 0xffffffff
        mov dword ptr s_writeP, 0xffffffff
        jmp eax

.globl SuperVUEndProgram
SuperVUEndProgram:
    // restore cpu state
    ldmxcsr g_sseMXCSR
        mov esi, s_vu1esi
        mov edi, s_vuedi
        mov ebx, s_vuebx

        mov esp, s_vu1esp

        call SuperVUCleanupProgram
        jmp [s_callstack] // so returns correctly

#if defined(__POSIX__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
