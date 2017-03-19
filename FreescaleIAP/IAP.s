; FreescaleIAP assembly functions
;
    AREA iap_main_asm_code, CODE, READONLY
    
;---------------------------------------------------------------------------
; iapEraseSector(FTFA_Type *FTFA, uint32_t address)
;   R0 = FTFA pointer
;   R1 = starting address

    EXPORT iapEraseSector
iapEraseSector
    ; save registers
    STMFD   R13!,{R1,R4,LR}
    
    ; wait for any previous command to complete
    BL      iapWait
    
    ; clear any errors
    BL      iapClearErrors
    
    ; set up the command parameters
    MOVS    R4,#0
    STRB    R4,[R0,#1]   ; FTFA->FCNFG <- 0
    MOVS    R4,#9        ; command = erase sector (9)
    STRB    R4,[R0,#7]   ; FTFA->FCCOB0 <- command
    
    STRB    R1,[R0,#4]   ; FTFA->FCCOB3 <- address bits 16-23
    
    MOVS    R1,R1,LSR #8 ; address >>= 8
    STRB    R1,[R0,#5]   ; FTFA->FCCOB2 <- address bits 8-15
    
    MOVS    R1,R1,LSR #8 ; address >>= 8
    STRB    R1,[R0,#6]   ; FTFA->FCCOB1 <- address bits 0-7
    
    ; execute (and wait for completion)
    BL      iapExecAndWait
    
    ; pop registers and return
    LDMFD   R13!,{R1,R4,PC}    

;---------------------------------------------------------------------------
; iapProgramBlock(TFA_Type *ftfa, uint32_t address, const void *src, uint32_t length)
;   R0 = FTFA pointer
;   R1 = flash address
;   R2 = source data pointer
;   R3 = data length in bytes

    EXPORT iapProgramBlock
iapProgramBlock
    ; save registers
    STMFD   R13!, {R1,R2,R3,R4,LR}
    
    ; wait for any previous command to complete
    BL      iapWait
    
    ; iterate over the data
LpLoop
    CMPS    R3,#3        ; at least one longword left (>= 4 bytes)?
    BLS     LpDone       ; no, done
    
    ; clear any errors from the previous command
    BL      iapClearErrors
    
    ; set up the command parameters
    MOVS    R4,#0
    STRB    R4,[R0,#1]   ; FTFA->FCNFG <- 0
    MOVS    R4,#6        ; command = program longword (6)
    STRB    R4,[R0,#7]   ; FTFA->FCCOB0 <- command
    
    MOVS    R4,R1        ; R4 <- current address
    STRB    R4,[R0,#4]   ; FTFA->FCCOB3 <- address bits 16-23
    
    MOVS    R4,R4,LSR #8 ; address >>= 8
    STRB    R4,[R0,#5]   ; FTFA->FCCOB2 <- address bits 8-15

    MOVS    R4,R4,LSR #8 ; address >>= 8    
    STRB    R4,[R0,#6]   ; FTFA->FCCOB1 <- address bits 0-7
    
    LDRB    R4,[R2]      ; R4 <- data[0]
    STRB    R4,[R0,#8]   ; FTFA->FCCOB7 <- data[0]
    
    LDRB    R4,[R2,#1]   ; R4 <- data[1]
    STRB    R4,[R0,#9]   ; FTFA->FCCOB6 <- data[1]
    
    LDRB    R4,[R2,#2]   ; R4 <- data[2]
    STRB    R4,[R0,#0xA] ; FTFA->FCCOB5 <- data[2]
    
    LDRB    R4,[R2,#3]   ; R4 <- data[3]
    STRB    R4,[R0,#0xB] ; FTBA->FCCOB4 <- data[3]
    
    ; execute the command
    BL      iapExecAndWait
    
    ; advance to the next longword
    ADDS    R1,R1,#4     ; flash address += 4
    ADDS    R2,R2,#4     ; source data pointer += 4
    SUBS    R3,R3,#4     ; data length -= 4
    B       LpLoop       ; back for the next iteration
    
LpDone    
    ; pop registers and return
    LDMFD   R13!, {R1,R2,R3,R4,PC}    

    
;---------------------------------------------------------------------------
; iapClearErrors(FTFA_Type *FTFA) - clear errors from previous command
;   R0 = FTFA pointer

iapClearErrors
    ; save registers
    STMFD   R13!, {R2,R3,LR}

    LDRB    R2, [R0]    ; R2 <- FTFA->FSTAT
    MOVS    R3, #0x30   ; FPVIOL (0x10) | ACCERR (0x20)
    ANDS    R2, R2, R3  ; R2 &= error bits
    BEQ     Lc0         ; if all zeros, no need to reset anything
    STRB    R2, [R0]    ; write the 1 bits back to clear the error status
Lc0
    ; restore registers and return
    LDMFD   R13!, {R2,R3,PC}    


;---------------------------------------------------------------------------
; iapWait(FTFA_Type *FTFA) - wait for command to complete
;   R0 = FTFA pointer

iapWait
    ; save registers
    STMFD   R13!, {R1,R2,LR}

    ; the CCIF bit is SET when the command completes
Lw0
    LDRB    R1, [R0]     ; R1 <- FTFA->FSTAT
    MOVS    R2, #0x80    ; CCIF (0x80)
    TSTS    R1, R2       ; test R1 & CCIF
    BEQ     Lw0          ; if zero, the command is still running

    ; pop registers and return
    LDMFD   R13!, {R1,R2,PC}    


;---------------------------------------------------------------------------
;
; The iapExecAndWait function MUST NOT BE IN FLASH, since we can't have
; any flash reads occur while an erase or write operation is executing.  If
; the code were in flash, the CPU might have to fetch an instruction from
; flash in the course of the loop, which could freeze the CPU.  Force the
; linker to put this section in RAM by making it read-write.

    AREA iap_ram_asm_code, CODE, READWRITE

;---------------------------------------------------------------------------
;
; iapExecAndWait(FTFA_Type *FTFA)
;   R0 = FTFA pointer
;
; This sets the bit in the FTFA status register to launch execution
; of the command currently configured in the control registers.  The
; caller must set up the control registers with the command code, and
; any address data parameters requied for the command.  After launching
; the command, we loop until the FTFA signals command completion.
;
; This routine turns off CPU interrupts and disables all peripheral
; interrupts through the NVIC while the command is executing.  That
; should eliminate any possibility of a hardware interrupt triggering
; a flash fetch during a programming operation.  We restore interrupts
; on return.  The caller doesn't need to (and shouldn't) do its own
; interrupt manipulation.  In testing, it seems problematic to leave
; interrupts disabled for long periods, so the safest approach seems
; to be to disable the interrupts only for the actual command execution.

    EXPORT iapExecAndWait
iapExecAndWait
    ; save registers
    STMFD   R13!, {R1,R2,R3,R4,LR}
    
    ; disable all interrupts in the NVIC
    LDR     R3, =NVIC_ICER ; R3 <- NVIC_ICER
    LDR     R4, [R3]     ; R4 <- current interrupt status
    MOVS    R2, #0       ; R2 <- 0
    SUBS    R2,R2,#1     ; R2 <- 0 - 1 = 0xFFFFFFFF
    STR     R2, [R3]     ; [NVIC_ICER] <- 0xFFFFFFFF (disable all interrupts)

    ; disable CPU interrupts
    CPSID I              ; interrupts off
    DMB                  ; data memory barrier
    DSB                  ; data synchronization barrier
    ISB                  ; instruction synchronization barrier

    ; Launch the command by writing the CCIF bit to FTFA_FSTAT    
    MOVS    R1, #0x80    ; CCIF (0x80)
    STRB    R1, [R0]     ; FTFA->FSTAT = CCIF
    
    ; Wait for the command to complete.  The FTFA sets the CCIF
    ; bit in FTFA_FSTAT when the command is finished, so spin until
    ; the bit reads as set.
Lew0
    LDRB    R1, [R0]     ; R1 <- FTFA->FSTAT
    MOVS    R2, #0x80    ; CCIF (0x80)
    TSTS    R1, R2       ; test R1 & CCIF
    BEQ     Lew0         ; if zero, the command is still running
    
    ; restore CPU interrupts
    CPSIE I

    ; re-enable NVIC interrupts
    LDR     R3, =NVIC_ISER ; R3 <- NVIC_ISER
    STR     R4, [R3]     ; NVIC_ISER = old interrupt enable vector
    
    ; pop registers and return
    LDMFD   R13!, {R1,R2,R3,R4,PC}

    ALIGN
NVIC_ISER  DCD 0xE000E100
NVIC_ICER  DCD 0xE000E180

    END
