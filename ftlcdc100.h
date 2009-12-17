/*
 * Faraday FTLCDC100 LCD Controller
 *
 * (C) Copyright 2009 Faraday Technology
 * Po-Yu Chuang <ratbert@faraday-tech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __FTLCDC100_H
#define __FTLCDC100_H

#define FTLCDC100_OFFSET_LCD_HTIMING		0x00
#define FTLCDC100_OFFSET_LCD_VTIMING		0x04
#define FTLCDC100_OFFSET_LCD_CLOCK_POLARITY	0x08
#define FTLCDC100_OFFSET_LCD_FRAME_BASE		0x10
#define FTLCDC100_OFFSET_LCD_INT_ENABLE		0x18
#define FTLCDC100_OFFSET_LCD_CONTROL		0x1c
#define FTLCDC100_OFFSET_LCD_INT_CLEAR		0x20
#define FTLCDC100_OFFSET_LCD_INT_STATUS		0x24
#define FTLCDC100_OFFSET_OSD_SCALING_CONTROL	0x34
#define FTLCDC100_OFFSET_OSD_POSITION_CONTROL	0x38
#define FTLCDC100_OFFSET_OSD_FG_CONTROL		0x3c
#define FTLCDC100_OFFSET_OSD_BG_CONTROL		0x40
#define FTLCDC100_OFFSET_GPIO_CONTROL		0x44
#define FTLCDC100_OFFSET_PALETTE		0x200	/* 0x200  - 0x3fc */
#define FTLCDC100_OFFSET_OSD_FONT		0x8000	/* 0x8000 - 0xbffc */
#define FTLCDC100_OFFSET_OSD_ATTRIBUTE		0xc000	/* 0xc000 - 0xc7fc */

/*
 * LCD Horizontal Timing Control
 */
#define FTLCDC100_LCD_HTIMING_PL(x)	(((x) & 0x3f) << 2)
#define FTLCDC100_LCD_HTIMING_HW(x)	(((x) & 0xff) << 8)	/* HW determines X-Position */
#define FTLCDC100_LCD_HTIMING_HFP(x)	(((x) & 0xff) << 16)
#define FTLCDC100_LCD_HTIMING_HBP(x)	(((x) & 0xff) << 24)

/*
 * LCD Vertial Timing Control
 */
#define FTLCDC100_LCD_VTIMING_LF(x)	(((x) & 0x3ff) << 0)
#define FTLCDC100_LCD_VTIMING_VW(x)	(((x) & 0x3f) << 10)	/* VW determines Y-Position */
#define FTLCDC100_LCD_VTIMING_VFP(x)	(((x) & 0xff) << 16)
#define FTLCDC100_LCD_VTIMING_VBP(x)	(((x) & 0xff) << 24)

/*
 * LCD Clock and Signal Polarity Control
 */
#define FTLCDC100_LCD_CLOCK_POLARITY_DIVNO(x)	((x) & 0x3f)	/* bus clock rate / (x + 1) determines the frame rate */
#define FTLCDC100_LCD_CLOCK_POLARITY_IVS	(1 << 11)
#define FTLCDC100_LCD_CLOCK_POLARITY_IHS	(1 << 12)
#define FTLCDC100_LCD_CLOCK_POLARITY_ICK	(1 << 13)
#define FTLCDC100_LCD_CLOCK_POLARITY_IDE	(1 << 14)
#define FTLCDC100_LCD_CLOCK_POLARITY_ADPEN	(1 << 15)

/*
 * LCD Panel Frame Base Address
 */
#define FTLCDC100_LCD_FRAME_BASE_FRAME420_SIZE(x)	((x) & 0x3c)
#define FTLCDC100_LCD_FRAME_BASE(x)			((x) & ~0x3f)

/*
 * LCD Panel Pixel Parameters
 */
#define FTLCDC100_LCD_CONTROL_ENABLE		(1 << 0)
#define FTLCDC100_LCD_CONTROL_BPP1		(0x0 << 1)
#define FTLCDC100_LCD_CONTROL_BPP2		(0x1 << 1)
#define FTLCDC100_LCD_CONTROL_BPP4		(0x2 << 1)
#define FTLCDC100_LCD_CONTROL_BPP8		(0x3 << 1)
#define FTLCDC100_LCD_CONTROL_BPP16		(0x4 << 1)
#define FTLCDC100_LCD_CONTROL_BPP24		(0x5 << 1)
#define FTLCDC100_LCD_CONTROL_TFT		(1 << 5)
#define FTLCDC100_LCD_CONTROL_BGR		(1 << 8)
#define FTLCDC100_LCD_CONTROL_LEB_LEP		(0x0 << 9)	/* little endian byte, little endian pixel */
#define FTLCDC100_LCD_CONTROL_BEB_BEP		(0x1 << 9)	/* big    endian byte, big    endian pixel */
#define FTLCDC100_LCD_CONTROL_LEB_BEP		(0x2 << 9)	/* little endian byte, big    endian pixel */
#define FTLCDC100_LCD_CONTROL_LCD		(1 << 11)
#define FTLCDC100_LCD_CONTROL_VSYNC		(0x0 << 12)
#define FTLCDC100_LCD_CONTROL_VBACK		(0x1 << 12)
#define FTLCDC100_LCD_CONTROL_VACTIVE		(0x2 << 12)
#define FTLCDC100_LCD_CONTROL_VFRONT		(0x3 << 12)
#define FTLCDC100_LCD_CONTROL_PANEL_TYPE	(1 << 15)
#define FTLCDC100_LCD_CONTROL_FIFO_THRESHOLD	(1 << 16)
#define FTLCDC100_LCD_CONTROL_YUV420		(1 << 17)
#define FTLCDC100_LCD_CONTROL_YUV		(1 << 18)

/*
 * LCD Interrupt Enable/Status/Clear
 */
#define FTLCDC100_LCD_INT_UNDERRUN		(1 << 1)
#define FTLCDC100_LCD_INT_NEXT_BASE		(1 << 2)
#define FTLCDC100_LCD_INT_VSTATUS		(1 << 3)
#define FTLCDC100_LCD_INT_BUS_ERROR		(1 << 4)

#endif	/* __FTLCDC100_H */
