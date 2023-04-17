;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                               ;
; APPLE II SSC FIRMWARE         ;
;                               ;
;   BY LARRY KENYON             ;
;                               ;
;    -JANUARY 1981-             ;;;;;;;;;;;;;;
;                                            ;
; (C) COPYRIGHT 1981 BY APPLE COMPUTER, INC. ;
;                                            ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                               ;
; CN00 SPACE CODE               ;
;                               ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
        ORG $C000

.IFDEF SLOT
    .DEFINE REPEAT 0
.ELSE
    .DEFINE REPEAT 1
.ENDIF

.IF REPEAT
    .REPEAT 7,COUNT
        .IF COUNT
            .SCOPE
        .ENDIF
.ENDIF

slot  = COUNT + 1

.segment .sprintf("io_sel%d",slot)

BINIT   BIT IORTS       ; SET THE V-FLAG
        BVS BENTRY      ; <ALWAYS>
IENTRY  SEC             ; BASIC INPUT ENTRY
        DFB $90         ; OPCODE FOR BCC
OENTRY  CLC             ; BASIC OUTPUT ENTRY
        CLV
        BVC BENTRY      ; <ALWAYS> SKIP AROUND PASCAL 1.1 ENTRY
        DFB $01         ; GENERIC SIGNATURE BYTE
        DFB $31         ; DEVICE SIGNATURE BYTE
        DFB <PINIT
        DFB <PREAD
        DFB <PWRITE
        DFB <PSTATUS
BENTRY  STA CHARACTER
        STX ZPTEMP      ; INPUT BUFFER INDEX
        TXA             ; SAVE X AND Y REGS ON STACK
        PHA
        TYA
        PHA
        PHP             ; SAVE ENTRY FLAGS
        SEI             ; NO RUPTS DURING SLOT DETERMINATION
        STA ROMSOFF     ; SWITCH OUT OTHER $C800 ROMS
        JSR IORTS
        TSX
        LDA STACK,X     ; RECOVER $CN
        STA MSLOT
        TAX             ; X-REG WILL GENERALLY BE $CN
        ASL A
        ASL A           ; DETERMINE $N0
        ASL A
        ASL A
        STA SLOT16
        TAY             ; Y-REG WILL GENERALLY BE $N0
        PLP             ; RESTORE RUPTS
        BVC NORMIO
;
; BASIC INITIALIZATION
;
        ASL CMDBYTE,X   ; ALWAYS ENABLE COMMANDS
        LSR CMDBYTE,X
        LDA CMDREG,Y    ; JUST HAD A POWER-ON OR PROGRAM RESET?
        AND #$1F
        BNE BINIT1
        LDA #$EF        ; IF SO, GO JOIN INIT IN PROGRESS
        JSR INIT1

BINIT1  CPX CSWH
        BNE FROMIN
        LDA #<OENTRY
        CMP CSWL        ; IF CSW IS ALREADY POINTING TO OENTRY,
        BEQ FROMIN      ;  THEN WE MUST HAVE COME FROM KSW
        STA CSWL        ; OTHERWISE, SET CSW TO OENTRY
FROMOUT CLC             ; INDICATE WE ARE CALLED FOR OUTPUT
        BCC NORMIO      ; <ALWAYS>
FROMIN  CPX KSWH        ; MAKE SURE KSW POINTS HERE
        BNE FROMOUT     ;
        LDA #<IENTRY
        STA KSWL        ; SET UP KSW (NOTE CARRY SET FROM CPX)
;
; BRANCH TO APPROPRIATE BASIC I/O ROUTINE
;
NORMIO  LDA MISCFLG,X   ; SEPARATE CIC MODE FROM OTHERS
        AND #$02        ; NOT ZERO FOR CIC MODE
        PHP             ; SAVE CIC MODE INDICATION
        BCC BOUTPUT
        JMP BINPUT

BOUTPUT LDA STATEFLG,X  ; CHECK FOR AFTER LOWERCASE INPUT
        PHA
        ASL A
        BPL BOUTPUT1    ; SKIP IF NOT
        LDX ZPTEMP
        LDA CHARACTER
        ORA #$20
        STA INBUFF,X    ; RESTORE LOWERCASE IN BUFFER
        STA CHARACTER   ; AND FOR OUTPUT ECHO
        LDX MSLOT
BOUTPUT1 PLA
        AND #$BF        ; ZERO THE FLAG
        STA STATEFLG,X
        PLP             ; RETRIEVE CIC MODE INDICATION
        BEQ BOUTPUT2    ; BRANCH FOR PPC, SIC MODES
        JSR OUTPUT      ; CIC MODE OUTPUT
        JMP CICEXIT     ; FINISH BY CHECKING FOR TERM MODE

BOUTPUT2 JMP SEROUT
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                                ;
;  NEW PASCAL INTERFACE ENTRIES  ;
;                                ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
PINIT   JSR PASCALINIT
        LDX #0          ; NO ERROR POSSIBLE
        RTS
PREAD   JMP PASCALREAD
PWRITE  JMP PASCALWRITE
;
; NEW PASCAL STATUS REQUEST
;
; A-REG=0 -> READY FOR OUTPUT?
; A-REG=1 -> HAS INPUT BEEN RECEIVED?
;
PSTATUS LSR A           ; SAVE REQUEST TYPE IN CARRY
        JSR PENTRY      ; (PRESERVES CARRY)
        BCS PSTATIN
        JSR SROUT       ; READY FOR OUTPUT?
        BEQ PSTATUS2
        CLC
        BCC PSTATUS2    ; CARRY CLEAR EOR NOT READY

PSTATIN JSR SRIN        ; SETS CARRY CORRECTLY
PSTATUS2 LDA STSBYTE,X  ; GET ERROR FLAGS
        TAX
        RTS
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; ROUTINE TO SEND A CHARACTER TO ANOTHER CARD ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
SENDCD  LDX #3
SAVEHOOK LDA CSWL,x
        PHA
        DEX
        BPL SAVEHOOK
;
; NOW PUT CARD ADDRESS IN HOOK
;
        LDX MSLOT
        LDA CHNBYTE,X
        STA CSWL
        LDA STATEFLG,X  ; GET SLOT #
        AND #$38
        LSR A
        LSR A
        LSR A
        ORA #$C0        ; FORM $CN
        STA CSWH
;
; OUTPUT TO THE PERIPHERAL
;
        TXA             ; SAVE $CN
        PHA
        LDA CHARACTER
        PHA
        ORA #$80        ; 80 COL BOARDS WANT HI-BIT ON
        JSR COUT
;
; NOW RESTORE EVERYTHING THE OTHER CARD MAY HAVE CLOBBERED
;
        PLA
        STA CHARACTER
        PLA
        STA MSLOT
        TAX
        ASL A
        ASL A
        ASL A
        ASL A
        STA SLOT16
        STA ROMSOFF
;
; PUT BACK CSWL INTO CHNBYTE
;
        LDA CSWL
        STA CHNBYTE,X

        LDX #0
RESTORHOOK PLA
        STA CSWL,X
        INX
        CPX #4
        BCC RESTORHOOK

        LDX MSLOT
        RTS

        ASC "APPLE"
        DFB $8

.IF REPEAT
        .IF COUNT
            .ENDSCOPE
        .ENDIF
    .ENDREP
.ENDIF
