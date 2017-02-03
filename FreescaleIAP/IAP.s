; FreescaleIAP assembly functions
;
; Put all code here in READWRITE memory, to ensure that it goes in RAM 
; rather than flash.  
    AREA iapexec_asm_code, CODE, READWRITE
    
;---------------------------------------------------------------------------
; iapEraseSector(FTFA_Type *FTFA, uint32_t address)
;   R0 = FTFA pointer
;   R1 = starting address

    EXPORT iapEraseSector
iapEraseSector
    ; save registers
    STMFD   R13!,{R1,R4,LR}
    
    ; Ensure that no interrupts occur while we're erasing.  This is
    ; vitally important, because the flash controller doesn't allow
    ; anyone to read from flash while an erase is in progress.  Most
    ; of the program code is in flash, which means that any interrupt
    ; would invoke flash code, causing the CPU to fetch from flash,
    ; violating the no-read-during-erase rule.  The CPU instruction
    ; fetch would fail, causing the CPU to lock up.
    CPSID I              ; interrupts off
    DMB                  ; data memory barrier
    ISB                  ; instruction synchronization barrier
    
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
    
    ; execute and wait for completion
    BL      iapExec
    BL      iapWait
    
    ; restore interrupts
    CPSIE I

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
    
    ; Turn off interrupts while we're working.  Flash reading
    ; while writing doesn't seem to be forbidden the way it is
    ; while erasing (see above), but even so, each longword
    ; transfer requires writing to 10 separate registers, so
    ; there's a lot of static state involved - we don't want
    ; any other code sneaking in and changing anything on us.
    CPSID I              ; interrupts off
    DMB                  ; data memory barrier
    ISB                  ; instruction synchronization barrier
    
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
    BL      iapExec
    
    ; advance to the next longword
    ADDS    R1,R1,#4     ; flash address += 4
    ADDS    R2,R2,#4     ; source data pointer += 4
    SUBS    R3,R3,#4     ; data length -= 4
    B       LpLoop       ; back for the next iteration
    
LpDone    
    ; restore interrupts
    CPSIE I
    
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

LwDone
    ; pop registers and return
    LDMFD   R13!, {R1,R2,PC}    


;---------------------------------------------------------------------------
; iapExec(FTFA_Type *FTFA)
;   R0 = FTFA pointer

iapExec
    ; save registers
    STMFD   R13!, {R1,LR}
        
    ; write the CCIF bit to launch the command
    MOVS    R1, #0x80    ; CCIF (0x80)
    STRB    R1, [R0]     ; FTFA->FSTAT = CCIF
    
    ; wait until command completed
    BL      iapWait

    ; pop registers and return
    LDMFD   R13!, {R1,PC}    
    
    END
    