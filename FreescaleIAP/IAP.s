; Put this in READWRITE memory, to ensure that it goes in RAM 
; rather than flash.  
    AREA iapexec_asm_code, CODE, READWRITE

; iapExecAsm(FSTAT)
;   FSTAT = address of ftfa->FSTAT register
;
; Note: arguments passed in R0, R1...
    EXPORT iapExecAsm
iapExecAsm
    ; Do a layered call to our main handler routine.  Advice on ARM
    ; forums suggests that before we carry out the Flash write, we should
    ; do an extra call layer from one RAM-resident code block to another 
    ; RAM-resident code block, with the inner block firing off the actual
    ; FTFA execution.  This supposedly will reduce the chances of core
    ; lockup during the write.  This strikes me as probably superstitious
    ; in origin ("it was crashing and then I made some random changes to
    ; the code that I can't remember and also knocked three times on the 
    ; desk and then it worked, so always knock three times on a desk").
    ; But I have indeed seen random crashes, and this is only a few bytes
    ; of extra code, so what the heck.
    ;
    ; If there's a real justification for this, it's probably that the
    ; extra call fills up the CPU instruction fetch/pre-fetch mechanism 
    ; with RAM addresses, flushing out any remaining Flash addresses that 
    ; might trigger an asynchronous Flash read from the CPU instruction 
    ; fetch mechanism.  The big hazard with programming Flash through the 
    ; FTFA is that any read access to the Flash will fail while a write
    ; operation is in progress.  This will cause a CPU lockup if such a
    ; read comes from the instruction fetcher.
    ;
    ; Simply push the link register, call our main routine, and return.
    ; Arguments are in registers (R0, R1, ...), so they'll just pass
    ; through to the callee.
    STMFD   R13!, {LR}
    BL      iapExecMain
    LDMFD   R13!, {PC}  

;
; Main routine
;
iapExecMain
    ; push R1, R2, link
    STMFD   R13!, {R1,R2,LR}
    
    ; Advice on ARM forums suggests that we should add a little artificial
    ; delay here to avoid core lockup.  The point seems to be to reduce the
    ; chances that a bus operation related to instruction fetching will hit 
    ; the Flash while the write operation is executing.  This seems as
    ; superstitious as the extra layered call (see above), but just in case...
    MOVS    R1, #100    ; loop counter
L0
    SUBS    R1, R1, #1  ; R1 = R1 - 1
    BNE     L0          ; loop until we reach 0    
        
    ; NB - caller is responsible for doing this
    ; clear old errors from status bits 
    ;MOVS    R1, #0x70   ; FPVIOL (0x10) | ACCERR (0x20) | RDCOLOERR (0x40)
    ;STRB    R1, [R0]
    
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
    