;-----------------------------------------------------------------------------
; pce
;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; File name:    m24fix.asm
; Created:      2017-09-26 by Hampa Hug <hampa@hampa.ch>
; Copyright:    (C) 2017-2018 Hampa Hug <hampa@hampa.ch>
;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; This program is free software. You can redistribute it and / or modify it
; under the terms of the GNU General Public License version 2 as  published
; by the Free Software Foundation.
;
; This program is distributed in the hope  that  it  will  be  useful,  but
; WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General
; Public License for more details.
;-----------------------------------------------------------------------------


; The Olivetti M24 BIOS version 1.0 contains a bug in the int 0x15
; handler. It returns with a near ret instead of a far ret, resulting
; in a crash. This TSR fixes this by replacing the int 0x15 handler.


section .text

	org	0x0100

entry:
	jmp	start


;-----------------------------------------------------------------------------
; INT 15 handler
;-----------------------------------------------------------------------------
newint15:
	push	bp
	mov	bp, sp

	or	byte [bp + 6], 1	; Set the carry flag
	mov	ah, 0x86		; Unsupported function

	pop	bp
	iret

res_end:


msg1	db	"Olivetti M24 BIOS 1.0 fix$"
msg2	db	" installed", 0x0d, 0x0a, "$"
msg3	db	" not installed", 0x0d, 0x0a, "$"

compare:
	db	0xf9, 0xb4, 0x86, 0xc2, 0x02, 0x00
compare_end:


;-----------------------------------------------------------------------------
; Initialization
;-----------------------------------------------------------------------------
start:
	mov	ax, cs
	mov	ds, ax

	cld

	mov	ah, 0x09
	mov	dx, msg1
	int	0x21			; print message

	mov	ax, 0x3515
	int	0x21			; get int 0x15 address

	mov	di, bx
	mov	si, compare
	mov	cx, compare_end - compare
	repe	cmpsb			; check old interrupt routine
	jne	.noinst

	mov	ax, 0x2515
	mov	dx, newint15
	int	0x21			; set int 0x15 address

	mov	ah, 0x09
	mov	dx, msg2
	int	0x21			; print message (installed)

	mov	ax, 0x3100
	mov	dx, (0x100 + res_end - entry + 15) / 16
	int	0x21			; terminate and stay resident
	jmp	.notreached

.noinst:
	mov	ah, 0x09
	mov	dx, msg3
	int	0x21			; print message (not installed)

	mov	ax, 0x4c01
	int	0x21			; terminate

.notreached:
	jmp	.notreached
