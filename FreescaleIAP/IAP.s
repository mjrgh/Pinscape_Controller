; Put this in READWRITE memory, to ensure that it goes in RAM 
; rather than flash.  
    AREA iapexec_asm_code, CODE, READWRITE

; iapExecAsm(FSTAT)
;   FSTAT = address of ftfa->FSTAT register
;
; Note: arguments passed in R0, R1...
    EXPORT iapExecAsm
iapExecAsm

    ; push R1, R2, link
    STMFD   R13!, {R1,R2,LR}
        
    ; clear old errors from status bits
    MOVS    R1, #0x70   ; FPVIOL (0x10) | ACCERR (0x20) | RDCOLOERR (0x40)
    STRB    R1, [R0]
    
    ; start command
    MOVS    R1, #0x80   ; CCIF (0x80)
    STRB    R1, [R0]
    
    ; wait until command completed - the CCIF bit is SET when the command completes
    MOVS    R2, #0x80   ; CCIF (0x80)
L1
    LDRB    R1, [R0]
    TSTS    R1, R2      ; CCIF (0x80)
    BEQ     L1

    ; pop registers and return
    LDMFD   R13!, {R1,R2,PC}    
    
    END
    