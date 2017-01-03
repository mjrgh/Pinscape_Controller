; CCD sensor scan mode 2
; This is an assembly language implementation of "scan mode 2",
; described more fully in ccdSensor.h.  This scan mode searches
; for the steepest edge in the pixel array, averaging over a few
; pixels on each side of the edge.  The assembly version is 
; necessary because the best C++ implementation I can come up
; with is too slow (about 12ms run time); this assembly version
; runs in about 1.5ms.

    AREA ccdScanMode2_asm, CODE, READONLY

; void ccdScanMode2(const uint8_t *pix, int npix, const uint8_t **edgep, int dir)
;    R0 = pix     = pointer to first byte of pixel array
;    R1 = npix    = number of pixels in the array
;    R2 = edgep   = filled in with pixel index of best edge on return, 
;                   or null if no edge was found
;    R3 = dir     = direction: 1 = bright region starts at first pixel,
;                             -1 = bright region starts at last pixel
;
; Note: arguments passed in R0, R1,... per ARM conventions.

    EXPORT ccdScanMode2
ccdScanMode2

    ; save used registers plus return link
    STMFD   R13!, {R4-R6,LR}

    ; set up registers:
    ;   R0 = current pixel index
    ;   R1 = end pixel index
    ;   R4 = running total
    ;   R5 = minmax so far
    ;   R6 = tmp (scratch register)
    ADDS    R1, R0, R1      ; endPix = pix + npix
    SUBS    R1, R1, #20     ; endPix -= pixel window size * 2
    MOVS    R5, #0          ; minmax = 0
    STR     R5, [R2]        ; *edgep = null - no edge found yet

    ; Figure sum(right window pixels) - sum(left window pixels).  
    ; We'll this as a running total.  On each iteration, we'll
    ; subtract the outgoing left pixel (because it comes out of
    ; the positive left sum), add the pixel on the border (since
    ; it's coming out of the negative right sum and going into the
    ; positive left sum), and subtract the incoming right pixel
    ; (since it's going into the negative right sum).
    
    ; figure the right sum
    LDRB    R4, [R0,#10]
    LDRB    R6, [R0,#11]
    ADDS    R4, R4, R6
    LDRB    R6, [R0,#12]
    ADDS    R4, R4, R6
    LDRB    R6, [R0,#13]
    ADDS    R4, R4, R6
    LDRB    R6, [R0,#14]
    ADDS    R4, R4, R6
    LDRB    R6, [R0,#15]
    ADDS    R4, R4, R6
    LDRB    R6, [R0,#16]
    ADDS    R4, R4, R6
    LDRB    R6, [R0,#17]
    ADDS    R4, R4, R6
    LDRB    R6, [R0,#18]
    ADDS    R4, R4, R6
    LDRB    R6, [R0,#19]
    ADDS    R4, R4, R6

    ; subtract the left sum
    LDRB    R6, [R0,#0]
    SUBS    R4, R4, R6
    LDRB    R6, [R0,#1]
    SUBS    R4, R4, R6
    LDRB    R6, [R0,#2]
    SUBS    R4, R4, R6
    LDRB    R6, [R0,#3]
    SUBS    R4, R4, R6
    LDRB    R6, [R0,#4]
    SUBS    R4, R4, R6
    LDRB    R6, [R0,#5]
    SUBS    R4, R4, R6
    LDRB    R6, [R0,#6]
    SUBS    R4, R4, R6
    LDRB    R6, [R0,#7]
    SUBS    R4, R4, R6
    LDRB    R6, [R0,#8]
    SUBS    R4, R4, R6
    LDRB    R6, [R0,#9]
    SUBS    R4, R4, R6

    ; check which direction we're going
    CMPS    R3, #0
    BLT     ReverseScan
    
    ; R3 is now available for other uses.  Use it as the pointer to
    ; the best result so far.

    ; Forward scan: scanning from bright end to dark end, so look for
    ; steepest negative slope
ForwardScan
    CMPS    R4, R5          ; if slope < minmax
    BGE     L0
    MOVS    R5, R4          ; ...then minmax = slope
    MOVS    R3, R0          ; ...and minmaxIdx = curpix
L0
    ; update the window
    LDRB    R6, [R0,#0]     ; tmp = curpix[0]
    ADDS    R4, R4, R6      ; leftSum -= curpix[-10], but the running total is
                            ; rightSum - leftSum, so ADD this to the running total


    LDRB    R6, [R0,#10]    ; tmp = curpix[10]
    MOVS    R6, R6, LSL #1  ; tmp *= 2: we're subtracting the pixel from rightSum
                            ; and adding it to leftSum, but leftSum is negative in
                            ; the running total, so it's like we're subtracting it
                            ; twice, thus we double it
    SUBS    R4, R4, R6      ; running total -= curpix[10]*2

    LDRB    R6, [R0,#20]    ; tmp = curpix[20]
    ADDS    R4, R4, R6      ; rightSum += curPix[20]

    ; increment the index and loop
    ADDS    R0, R0, #1      ; curPix++
    CMPS    R0, R1          ; if curPix <= endPix
    BLS     ForwardScan     ; loop

    ; done
    B       Done

    ; Reverse scan: scanning from dark end to bright end, so look for
    ; steepest positive slope
ReverseScan
    CMPS    R4, R5          ; if slope > minmax
    BLE     L1
    MOVS    R5, R4          ; ...then minmax = slope
    MOVS    R3, R0          ; ...and minmaxIdx = curpix
L1
    ; update the window
    LDRB    R6, [R0,#0]     ; tmp = curpix[0]
    ADDS    R4, R4, R6      ; leftSum -= curpix[-10], but the running total is
                            ; rightSum - leftSum, so ADD this to the running total


    LDRB    R6, [R0,#10]    ; tmp = curpix[10]
    MOVS    R6, R6, LSL #1  ; tmp *= 2: we're subtracting the pixel from rightSum
                            ; and adding it to leftSum, but leftSum is negative in
                            ; the running total, so it's like we're subtracting it
                            ; twice, thus we double it
    SUBS    R4, R4, R6      ; running total -= curpix[10]*2

    LDRB    R6, [R0,#20]    ; tmp = curpix[20]
    ADDS    R4, R4, R6      ; rightSum += curPix[20]

    ; increment the index and loop
    ADDS    R0, R0, #1      ; curPix++
    CMPS    R0, R1          ; if curPix <= endPix
    BLS     ReverseScan     ; loop
    
Done
    ; presume failure - return false
    MOVS    R0, #0
    
    ; if we found an edge, adjust the index for the window offset
    CMPS    R5, #0          ; if minmax != 0, we found an edge
    BEQ     L2              ; nope, no edge
    ADDS    R3, #10         ; add the pixel window offset
    STR     R3, [R2]        ; store the result in *edgep
    MOVS    R0, #1          ; return true

L2
    ; done - pop registers and return
    LDMFD   R13!, {R4-R6,PC}

    END
