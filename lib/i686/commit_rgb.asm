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

bits 32

%ifidni __OUTPUT_FORMAT__,Win32
%define PIC
%define prefix _asar_
%else
%define PIC + ebx wrt ..gotoff
%define prefix asar_
%endif

section .rodata align=64

b0mask	times 4 dd  000000ffh
one0	times 4 dd  00010001h
one16	times 4 dd  01000100h

section .text

fg_active	equ	0
fg_w		equ	4
fg_h		equ	8
fg_unused	equ	12

l_first		equ	0
l_last		equ	4
l_data		equ	8

a_dptr		equ	8
a_stride	equ	12

%define	expect		db	3eh
%define	unexpect	db	2eh

global prefix %+ commit_rgbx_bgrx_SSE2
global prefix %+ commit_xrgb_xbgr_SSE2

%define xmmzero	xmm3
%macro ldxmm 2
	movd		%1, [edx+%2]
	punpcklbw	%1, xmmzero
	punpcklbw	%1, xmmzero
	packssdw	%1, %1
	paddusw		%1, [one0 PIC]
%endmacro

%macro rgb_committer 1
	push	ebp
	push	esi
	push	edi

	mov	edi, [esp+16]	; g
	mov	esi, [esp+20]	; lines
	mov	edx, [esp+24]	; colours

; originally there were more variables on the stack
	mov	ebp, esp
	sub	esp, 16
	and	esp, -64
	
%define espsave 12
%define goal	8
%define	unused	4
%define stride	0
	mov	[esp+espsave], ebp
	
	mov	ebp, [edi+fg_active]	; g->active

	mov	eax, dword [edi+fg_h]
	shl	eax, 2
	add	eax, esi
	mov	[esp+goal], eax

	mov	eax, [ebp+a_stride]
	mov	[esp+stride], eax
	
	mov	eax, [edi+fg_unused]
	mov	[esp+unused], eax

%define	s	edi
%define	d	ebp
	mov	d, [ebp+a_dptr]

	pxor	xmmzero, xmmzero

%define lownib xmm5
%define midnib xmm6
%define hignib xmm7
%define alpha  xmm4

	ldxmm lownib, 0
	ldxmm midnib, 4
	ldxmm hignib, 8

	movd		xmm2, [edx+12]
	punpcklbw	xmm2, xmmzero
	punpcklbw	xmm2, xmmzero
	packssdw	xmm2, xmm2
	movdqa		xmm4, [one16 PIC]
	psubusw		xmm4, xmm2
	
; main loop:
;	esi = lineptr
;	s(edi) = *lineptr (cellline)
;	d(ebp) = imgdata
	mov	edx, [esp+goal]
	mov	ecx, [esp+unused]
	mov	eax, [esp+stride]
.mainloop:
	cmp	esi, edx
	unexpect
	jz	.exit
	mov	s, [esi]
	cmp	s, ecx
	unexpect
	jnz	.process

.done:
	add	d, eax
	lea	esi, [esi+4]
	jmp	.mainloop

.exit:
	mov	esp, [esp+espsave]
	pop	edi
	pop	esi
	pop	ebp
	ret

.subdone1:
	mov	edx, [esp+goal]
	mov	ecx, [esp+unused]
	mov	eax, [esp+stride]
	jmp	.done

; tier-2 loop:
;	s(edi) = cellline, clobber
;	d(ebp) = imgdata, untouched
;	eax, ebx, edx = clobber

.process:
	mov	eax, [s+l_last]
	add	eax, 3
	and	eax, -2
	shl	eax, 2
	add	eax, s
	mov	ecx, eax

	add	s, 8

	mov	edx, d
	sub	edx, s
%define dsoff edx

	mov	eax, [s+l_first-8]
	and	eax, -2
	shl	eax, 2
	add	s, eax

	mov	eax, ecx

; inner loop.
;	s(edi) = celldata
;	  eax  =  ^- end
;	dsoff(edx) = imgdata-celldata

.subloop1:
	cmp		s, eax
	unexpect
	jae .subdone1

; xmm0: celldata (B), xmm(1-3): scratch

	movq		xmm0, [s]
	movdqa		xmm1, xmm0
	pand		xmm1, [b0mask PIC]
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
		pxor		xmmzero, xmmzero
	psubusb		xmm0, xmm1		;    4s   3s  2s 1


	punpcklbw	xmm0, xmmzero
	psllw		xmm0, 8
	pmulhuw		xmm0, alpha
; xmm0(W4): celldata*alpha, xmm(1-3): scratch

	movdqa		xmm2, xmm0
	psadbw		xmm2, xmmzero
		movdqa		xmm1, [one16 PIC]
	pshufhw		xmm2, xmm2, 0
	pshuflw		xmm2, xmm2, 0
; xmm0(W4): celldata*alpha, xmm2(W4=): E(cd*alpha)

	;movdqa		xmm1, [one16 PIC]
	psubusw		xmm1, xmm2

	movq		xmm2, [s+dsoff]
		psllw		xmm0, 8
	punpcklbw	xmm2, xmmzero
	psllw		xmm2, 8

	pmulhuw		xmm1, xmm2
; xmm0(W4): celldata*alpha, xmm1(W4): indata * (256-sumalpha)

	;psllw		xmm0, 8

	movdqa		xmm2, xmm0
	pmulhuw		xmm2, lownib
	psadbw		xmm2, xmmzero
%if %1
	psllq		xmm2, %1
%endif

	paddusb		xmm1, xmm2

	movdqa		xmm2, xmm0
	pmulhuw		xmm2, midnib
	psadbw		xmm2, xmmzero
		pmulhuw		xmm0, hignib
	psllq		xmm2, 16;+%1

		psadbw		xmm0, xmmzero
	paddusb		xmm1, xmm2
	
	;pmulhuw	xmm0, hignib
	;psadbw		xmm0, xmmzero
	psllq		xmm0, 32;+%1

	paddusb		xmm1, xmm0
	
	packuswb	xmm1, xmmzero
	
	movq		[s+dsoff], xmm1

	lea	s, [s+8]
	jmp	.subloop1
%endmacro


align	16
; void asar_commit_rgbx_bgrx(struct assp_fgroup *g, celline **lines, cell colours[4])
prefix %+ commit_rgbx_bgrx_SSE2:
	rgb_committer	0

align	16
; void asar_commit_xrgbx_xbgr(struct assp_fgroup *g, celline **lines, cell colours[4])
prefix %+ commit_xrgb_xbgr_SSE2:
	rgb_committer	16

