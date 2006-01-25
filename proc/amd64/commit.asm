;****************************************************************************
; asa: portable digital subtitle renderer
;****************************************************************************
; Copyright (C) 2006  David Lamparter
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
;***************************************************************************

bits 64

section .rodata align=64

b0mask	ddq 000000ff000000ff000000ff000000ffh
one0	ddq 00010001000100010001000100010001h

section .text

fg_active	equ	0
fg_w		equ	8
fg_h		equ	12
fg_unused	equ	16

a_yptr		equ	8
a_ystr		equ	16
a_uptr		equ	24
a_ustr		equ	32
a_vptr		equ	40
a_vstr		equ	48
a_red_x		equ	56
a_red_y		equ	57

l_first		equ	0
l_last		equ	4
l_data		equ	8

%define	expect		db	3eh
%define	unexpect	db	2eh

global asar_commit_y420_x86_64

align	16
; void asar_commit_y420_x86_64(struct assp_fgroup *g, celline **lines, cell colours[3])
asar_commit_y420_x86_64:
	push	rbp
;	mov	rdi, rdi	; g
;	mov	rsi, rsi	; lines
;	mov	rdx, rdx	; colours

	mov	r8, [rdi+fg_active]	; g->active

; put heavily used variables together on the stack so they fit in a cacheline
	mov	rbp, rsp
	and	rsp, -256

%define w	[rsp+40]
%define goal	[rsp+32]
%define	unused	[rsp+24]
%define stridev	[rsp+16]
%define strideu	[rsp+8]
%define stridey	[rsp+0]
	xor	rax, rax
	mov	eax, dword [rdi+fg_w]
	shl	rax, 2
	push	rax
	mov	eax, dword [rdi+fg_h]
	shl	eax, 3
	add	rax, rsi
	push	rax
	push	qword [rdi+fg_unused]
	push	qword [r8+a_vstr]
	push	qword [r8+a_ustr]
	push	qword [r8+a_ystr]

%define	s	rdi
%define	y	r8
%define u	r9
%define v	r10
	mov	v, [r8+a_vptr]
	mov	u, [r8+a_uptr]
	mov	y, [r8+a_yptr]

%define xmmzero	xmm7
	pxor	xmmzero, xmmzero

; initialize Y multipliers in xmm6
	movd		xmm5, [rdx]
	punpcklbw	xmm5, xmmzero
	punpcklbw	xmm5, xmmzero
	movdqa		xmm8, [b0mask wrt rip]
	psubusb		xmm8, xmm5
	packuswb	xmm8, xmm8
	psllq		xmm8, 8

	movd		xmm6, [rdx+4]
	movq		xmm5, xmm6
	psllq		xmm5, 32
	por		xmm6, xmm5
	punpcklbw	xmm6, xmmzero
	paddw		xmm6, [one0 wrt rip]
	pmulhuw		xmm6, xmm8
	paddw		xmm6, [one0 wrt rip]

	psrldq		xmm8, 1
	paddw		xmm8, [one0 wrt rip]


.mainloop:
	cmp	rsi, goal
	unexpect
	jz	.exit
	mov	s, [rsi]
	cmp	s, unused
	unexpect
	jnz	.process
;	mov	rax, [rsi+8]
;	cmp	rax, unused
;	unexpect
;	jnz	.process

;	add	y, stridey
.done:
	add	y, stridey
	add	u, strideu
	add	v, stridev
	lea	rsi, [rsi+8]
	jmp	.mainloop

.process:
	mov	eax, [s+l_first]
	and	rax, -4
	add	y, rax
	shl	rax, 2
	mov	r15, rax
	mov	eax, [s+l_last]
	add	rax, 3
	and	rax, -4
	mov	rcx, rax
	shl	rax, 2
	add	s, 8
	add	rax, s
	; rax = goal
	add	s, r15

.subloop1:
	movdqa		xmm5, [b0mask wrt rip]

	cmp		s, rax
	unexpect
	jz .subdone1

	movdqu		xmm0, [s]
	movdqa		xmm1, xmm0
	pand		xmm1, xmm5
	psllw		xmm1, 8
	paddusb		xmm0, xmm1		;    4,   3, 21, 1
	movdqa		xmm2, xmm0
	psrad		xmm2, 8
	pslld		xmm2, 16		;    3,  21,  0, 0
	paddusb		xmm0, xmm2		;   43, 321, 21, 1
	movdqa		xmm3, xmm2
	pslld		xmm3, 8
	paddusb		xmm0, xmm3		; 4321, 321, 21, 1
	psubusb		xmm0, xmm3		;   43s 321  21  1
	psubusb		xmm0, xmm2		;    4s   3s 21  1
	psubusb		xmm0, xmm1		;    4s   3s  2s 1

	movdqa		xmm1, xmm0
	punpcklbw	xmm0, xmmzero
	 punpckhbw	 xmm1, xmmzero
	psllq		xmm0, 8
	 psllq		 xmm1, 8
	movdqa		xmm2, xmm0 
	 movdqa		 xmm3, xmm1
	pmulhuw		xmm0, xmm6
	 pmulhuw	 xmm1, xmm6
	psadbw		xmm0, xmmzero
	 psadbw		 xmm1, xmmzero

	packuswb	xmm0, xmm1
	packuswb	xmm0, xmmzero

	pmulhuw		xmm2, xmm8
	 pmulhuw	 xmm3, xmm8
	psadbw		xmm2, xmmzero
	 psadbw		 xmm3, xmmzero
	packuswb	xmm2, xmm3
	psubusb		xmm5, xmm2
	packuswb	xmm5, xmmzero
	paddw		xmm5, [one0 wrt rip]

	movd		xmm3, [y]
	pxor		xmm2, xmm2
	punpcklbw	xmm2, xmm3
	pmulhuw		xmm2, xmm5
	paddw		xmm2, xmm0
	packuswb	xmm2, xmmzero
	movd		[y], xmm2

	lea	s, [s+16]
	lea	y, [y+4]
	jmp	.subloop1

.subdone1:
	sub	y, rcx
	jmp 	.done

.exit:
	mov	rsp, rbp
	pop	rbp
	ret

