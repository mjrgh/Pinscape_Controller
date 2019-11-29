; FreescaleIAP assembly functions
;
; The hardware manual warns that FTFA commands must be executed entirely
; from RAM code, since we can't have any flash reads occur while an erase 
; or write operation is executing.  If the code executing and waiting for
; the FTFA command were in flash, the CPU might have to fetch an instruction
; from flash in the course of the loop, which could freeze the CPU.  
; Empirically, it seems that this isn't truly necessary, despite the manual's
; warnings.  The M0+ instruction cache is big enough to hold the whole
; execute-and-wait loop instruction sequence, even when written in C++, so
; in practice this can run as flash-resident C++ code.  We're implementing
; it as assembler anyway to follow the best practices as laid out in the
; hardware manual.
;
; Tell the linker to put our code in RAM by making it read-write.
    AREA iap_ram_asm_code, CODE, READWRITE


; iapExecAndWait()
;
; Launches the currently loaded FTFA command and waits for completion.
; Before calling, the caller must set up the FTFA command registers with 
; the command code and any address and data parameters required.  The
; caller should also disable interrupts, since an interrupt handler could
; cause a branch into code resident in flash memory, which would violate
; the rule against accessing flash while an FTFA command is running.

    EXPORT iapExecAndWait
iapExecAndWait
    ; save registers
    STMFD   R13!, {R1,R2,LR}
    
    ; disable interrupts
    CPSID   I            ; set the PRIMASK to disable interrupts
    DSB                  ; data synchronization barrier
    ISB                  ; instruction synchronization barrier
    
    ; Launch the command by writing the CCIF bit to FTFA_FSTAT    
    LDR     R0, FTFA_FSTAT
    MOVS    R2, #0x80    ; CCIF (0x80)
    STRB    R2, [R0]     ; FTFA->FSTAT = CCIF
    
    ; Wait for the command to complete.  The FTFA sets the CCIF
    ; bit in FTFA_FSTAT when the command is finished, so spin until
    ; the bit reads as set.
Lew0
    LDRB    R1, [R0]     ; R1 <- FTFA->FSTAT
    TSTS    R1, R2       ; test R1 & CCIF
    BEQ     Lew0         ; if zero, the command is still running
    
    ; re-enable interrupts
    CPSIE   I
    
    ; pop registers and return
    LDMFD   R13!, {R1,R2,PC}

    ALIGN
FTFA_FSTAT DCD 0x40020000

    END
