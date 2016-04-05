;------------------------------------------------------------------------------
;
; Event ring
;
; Copyright (c) 2016 Gerhard Bertelsmann
;
;------------------------------------------------------------------------------
;
; This file is part of the CANBuster project.
;
; CANBuster is licensed under the CC BY-NC-SA 4.0.
;
; See file /LICENSE for details.
; 
;------------------------------------------------------------------------------

                LIST
                RADIX   DEC

;------------------------------------------------------------------------------
; event ring variables
;------------------------------------------------------------------------------

                CBLOCK
                EVENTTMP : 1		; tmp buffer
                EVENTCNT : 1		; event counter
                EVENTPP : 2		; event pointer
                EVENTPG : 2		; event pointer
                ENDC

;------------------------------------------------------------------------------
; Event ring
;------------------------------------------------------------------------------

EVENTBUF        CBLOCK 0x0400		; event ring
                EVENTBUFFER : 1024
                ENDC

;------------------------------------------------------------------------------
; Event ring macros
;------------------------------------------------------------------------------

INITEVENTRING
                CLRF	EVENTCNT	; event counter
                MOVLW	HIGH EVENTBUF	; set event pointer
                MOVF	EVENTPP,F
                MOVF	EVENTPG,F
                MOVLW	LOW EVENTBUF
                MOVF	EVENTPP+1,F
                MOVF	EVENTPG+1,F
                RETURN

PUTEVENT
                MOVFF	EVENTPP,FSR0H
                MOVFF	EVENTPP+1,FSR0L
                MOVFF	ID_H,POSTINC0
                MOVFF	ID_L,POSTINC0
                MOVFF	PIOTEMP16,POSTINC0
                MOVFF	PIOTEMP16+1,POSTINC0
                MOVF	ACTUAL,W
                ANDWF	MASK,W
                BTFSS	STATUS,Z
                MOVLW   1		; either W is zero or it's set to one

                XORLW	1
                MOVWF	POSTINC0
                XORLW	1
                MOVWF	POSTINC0
                CLRF	POSTINC0	; time - unused
                CLRF	POSTINC0

                MOVLW	0x04		; make sure that FSR is in the range 0x400 - 0x7ff
                IORWF	FSR0H,F
                MOVLW	0x07
                ANDWF	FSR0H,F
                MOVFF   FSR0H,EVENTPP
                MOVFF   FSR0L,EVENTPP+1

                INCF	EVENTCNT,F
                RETURN

GETEVENT_P      MACRO
                MOVFF	EVENTPG,FSR0H
                MOVFF	EVENTPG+1,FSR0L
                ENDM

GETEVENT_P_NORM MACRO
                DECF	EVENTCNT,F
                MOVLW	0x04		; make sure that FSR is in the range 0x400 - 0x7ff
                IORWF	FSR0H,F
                MOVLW	0x07
                ANDWF	FSR0H,F
                ENDM

_SENDEVENT_
		; check if an event is in the buffer
                MOVF	EVENTCNT,F
                BTFSC	STATUS,Z
                RETURN
		; check if UART send buffer is available
                MOVWF	TXCNT,W
                SUBLW	OVERFLOW - 7	; 6 buffers spare for CAN -> UART
                BTFSC	STATUS,C
                RETURN

                ; last: check for CAN buffer
		; Find a Tx slot - start with TX buffer 2 down to 1
                MOVLW   CAN_WIN_TXB2        ; Window mode on TXB2
                MOVWF   CANCON
                BTFSS   TXB2CON,TXREQ
                BRA     EVENTTXCAN

                MOVLW   CAN_WIN_TXB1        ; Window mode on TXB1
                MOVWF   CANCON
                BTFSS   TXB1CON,TXREQ
                BRA     EVENTTXCAN

                RETURN

EVENTTXCAN
                MOVLW   0
                TXEXT_28_24
                MOVLW   23h
                TXEXT_23_21
                MOVLW   23h
                TXEXT_20_18
                MOVLW   23h
                TXEXT_17_16
                BSF	RXB0SIDL,EXIDEN     ; Enable Extended Id
                MOVF	CANHASH_HIGH,W
                MOVWF	RXB0EIDH
                MOVF	CANHASH_LOW,W
                MOVWF	RXB0EIDL
                ; DLC = 8
                MOVLW	8
                MOVWF	RXB0DLC
                GETEVENT_P
                MOVFF	POSTINC0,RXB0D0
                MOVFF	POSTINC0,RXB0D1
                MOVFF	POSTINC0,RXB0D2
                MOVFF	POSTINC0,RXB0D3
                MOVFF	POSTINC0,RXB0D4
                MOVFF	POSTINC0,RXB0D5
                MOVFF	FSR0L,RXB0D6
                ; MOVFF	POSTINC0,RXB0D7
		MOVFF	EVENTCNT,RXB0D7
                BSF	RXB0CON,TXREQ       ; start CAN transmit

		; and now the SLCAN packet
                MOVLW	'T'
                PUTC
                MOVLW	00h
                CALL	BIN2ASC
                MOVLW	23h
                CALL	BIN2ASC
                MOVF	CANHASH_HIGH,W
                CALL	BIN2ASC
                MOVF	CANHASH_LOW,W
                CALL	BIN2ASC
                MOVLW	'8'
                PUTC
                GETEVENT_P
                MOVLW	8
                MOVWF	EVENTTMP
EVENTUARTLOOP
                MOVFF	POSTINC0,WREG
                PUTC
                DECFSZ	EVENTTMP,F
                BRA	EVENTUARTLOOP

                INCF	TXCNT,F
                BSF	UPIE,UTXIE		; enable Tx ring ISR

		; fix the pointer if necessary
                GETEVENT_P_NORM
                RETURN

;------------------------------------------------------------------------------
;
; vim: set shiftwidth=4 tabstop=4 softtabstop=4 expandtab
;