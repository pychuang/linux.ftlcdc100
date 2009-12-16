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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>

#include "ftlcdc100.h"

/*
 * Actually, I don't know if LC_CLK is AHB clock on A320.  It is not written
 * in A320 data sheet.
 */
#define LC_CLK	AHB_CLK_IN

/*
 * Andoird uses a virtual display twice the size of the physical screen and
 * use fb_pan_display() to achieve double buffering.
 */
#define CONFIG_NUMBER_OF_BUFFERS	2

/*
 * Select a panel configuration
 */
#undef CONFIG_SHARP_LQ057Q3DC02
#define CONFIG_AUO_A036QN01_CPLD
#undef CONFIG_PRIME_VIEW_PD035VX2

/* 
 * This structure defines the hardware state of the graphics card. Normally
 * you place this in a header file in linux/include/video. This file usually
 * also includes register information. That allows other driver subsystems
 * and userland applications the ability to use the same header file to 
 * avoid duplicate work and easy porting of software. 
 */
struct ftlcdc100 {
	struct resource *res;
	void *base;
	int irq;
	u32 pseudo_palette[16];
};

/**
 * ftlcdc100_default_fix - Default struct fb_fix_screeninfo
 * It is only used in ftlcdc100_probe, so mark it as __devinitdata
 */
static struct fb_fix_screeninfo ftlcdc100_default_fix __devinitdata = {
	.id		= "ftlcdc100",
	.type		= FB_TYPE_PACKED_PIXELS,
	.ypanstep	= 1,
	.accel		= FB_ACCEL_NONE,
};

/**
 * ftlcdc100_default_var - Default struct fb_var_screeninfo
 * It is only used in ftlcdc100_probe, so mark it as __devinitdata
 */

#ifdef CONFIG_SHARP_LQ057Q3DC02
static struct fb_var_screeninfo ftlcdc100_default_var __devinitdata = {
	.xres		= 320,
	.yres		= 240,
	.xres_virtual	= 320,
	.yres_virtual	= 240 * CONFIG_NUMBER_OF_BUFFERS,
	.bits_per_pixel	= 16,
	.pixclock	= 171521,
	.left_margin	= 17,
	.right_margin	= 17,
	.upper_margin	= 7,
	.lower_margin	= 15,
	.hsync_len	= 17,
	.vsync_len	= 1,
	.vmode		= FB_VMODE_NONINTERLACED,
	.sync		= FB_SYNC_VERT_HIGH_ACT,
};
#endif
#ifdef CONFIG_AUO_A036QN01_CPLD
static struct fb_var_screeninfo ftlcdc100_default_var __devinitdata = {
	.xres		= 320,
	.yres		= 240,
	.xres_virtual	= 320,
	.yres_virtual	= 240 * CONFIG_NUMBER_OF_BUFFERS,
	.bits_per_pixel	= 16,
	.pixclock	= 171521,
	.left_margin	= 44,
	.right_margin	= 6,
	.upper_margin	= 11,
	.lower_margin	= 8,
	.hsync_len	= 21,
	.vsync_len	= 3,
	.vmode		= FB_VMODE_NONINTERLACED,
	.sync		= 0,
};
#endif
#ifdef CONFIG_PRIME_VIEW_PD035VX2
static struct fb_var_screeninfo ftlcdc100_default_var __devinitdata = {
	.xres		= 640,
	.yres		= 480,
	.xres_virtual	= 640,
	.yres_virtual	= 480 * CONFIG_NUMBER_OF_BUFFERS,
	.bits_per_pixel	= 16,
	.pixclock	= 171521,
	.left_margin	= 44,
	.right_margin	= 20,
	.upper_margin	= 16,
	.lower_margin	= 16,
	.hsync_len	= 100,
	.vsync_len	= 19,
	.vmode		= FB_VMODE_NONINTERLACED,
	.sync		= 0,
};
#endif

/*
 * Modern graphical hardware not only supports pipelines but some 
 * also support multiple monitors where each display can have its  
 * its own unique data. In this case each display could be  
 * represented by a separate framebuffer device thus a separate 
 * struct fb_info. Now the struct xxx_par represents the graphics
 * hardware state thus only one exist per card. In this case the 
 * struct xxx_par for each graphics card would be shared between 
 * every struct fb_info that represents a framebuffer on that card. 
 * This allows when one display changes it video resolution (info->var) 
 * the other displays know instantly. Each display can always be
 * aware of the entire hardware state that affects it because they share
 * the same xxx_par struct. The other side of the coin is multiple
 * graphics cards that pass data around until it is finally displayed
 * on one monitor. Such examples are the voodoo 1 cards and high end
 * NUMA graphics servers. For this case we have a bunch of pars, each
 * one that represents a graphics state, that belong to one struct 
 * fb_info. Their you would want to have *par point to a array of device
 * states and have each struct fb_ops function deal with all those 
 * states. I hope this covers every possible hardware design. If not
 * feel free to send your ideas at jsimmons@users.sf.net 
 */

/******************************************************************************
 * interrupt handler
 *****************************************************************************/
static irqreturn_t ftlcdc100_interrupt(int irq, void *dev_id)
{
	struct fb_info *info = dev_id;
	struct ftlcdc100 *ftlcdc100 = info->par;
	unsigned int status;

	status = ioread32(ftlcdc100->base + FTLCDC100_OFFSET_LCD_INT_STATUS);

	if (status & FTLCDC100_LCD_INT_UNDERRUN) {
		if (printk_ratelimit())
			dev_notice(info->device, "underrun\n");
	}

	if (status & FTLCDC100_LCD_INT_NEXT_BASE) {
		if (printk_ratelimit())
			dev_dbg(info->device, "frame base updated\n");
	}

	if (status & FTLCDC100_LCD_INT_VSTATUS) {
		if (printk_ratelimit())
			dev_dbg(info->device, "vertical duration reached \n");

	}

	if (status & FTLCDC100_LCD_INT_BUS_ERROR) {
		if (printk_ratelimit())
			dev_err(info->device, "bus error!\n");

	}

	iowrite32(status, ftlcdc100->base + FTLCDC100_OFFSET_LCD_INT_CLEAR);

	return IRQ_HANDLED;
}

/******************************************************************************
 * struct platform_driver functions
 *****************************************************************************/
/**
 * ftlcdc100_check_var - Validates a var passed in.
 * @var: frame buffer variable screen structure
 * @info: frame buffer structure that represents a single frame buffer 
 *
 * Checks to see if the hardware supports the state requested by
 * var passed in. This function does not alter the hardware state!!! 
 * This means the data stored in struct fb_info and struct ftlcdc100 do 
 * not change. This includes the var inside of struct fb_info. 
 * Do NOT change these. This function can be called on its own if we
 * intent to only test a mode and not actually set it. The stuff in 
 * modedb.c is a example of this. If the var passed in is slightly 
 * off by what the hardware can support then we alter the var PASSED in
 * to what we can do.
 *
 * For values that are off, this function must round them _up_ to the
 * next value that is supported by the hardware.  If the value is
 * greater than the highest value supported by the hardware, then this
 * function must return -EINVAL.
 *
 * Exception to the above rule:  Some drivers have a fixed mode, ie,
 * the hardware is already set at boot up, and cannot be changed.  In
 * this case, it is more acceptable that this function just return
 * a copy of the currently working var (info->var). Better is to not
 * implement this function, as the upper layer will do the copying
 * of the current var for you.
 *
 * Note:  This is the only function where the contents of var can be
 * freely adjusted after the driver has been registered. If you find
 * that you have code outside of this function that alters the content
 * of var, then you are doing something wrong.  Note also that the
 * contents of info->var must be left untouched at all times after
 * driver registration.
 *
 * Returns negative errno on error, or zero on success.
 */
static int ftlcdc100_check_var(struct fb_var_screeninfo *var,
		struct fb_info *info)
{
	struct device *dev = info->device;
	unsigned long clk_value_khz;

	clk_value_khz = LC_CLK / 1000;

	dev_dbg(dev, "%s:\n", __func__);

	if (var->pixclock == 0) {
		dev_err(dev, "pixclock not specified\n");
		return -EINVAL;
	}

	dev_dbg(dev, "  resolution: %ux%u (%ux%u virtual)\n",
		info->var.xres, info->var.yres,
		info->var.xres_virtual, info->var.yres_virtual);
	dev_dbg(dev, "  pixclk:       %lu KHz\n", PICOS2KHZ(var->pixclock));
	dev_dbg(dev, "  bpp:          %u\n", var->bits_per_pixel);
	dev_dbg(dev, "  clk:          %lu KHz\n", clk_value_khz);
	dev_dbg(dev, "  left  margin: %u\n", var->left_margin);
	dev_dbg(dev, "  right margin: %u\n", var->right_margin);
	dev_dbg(dev, "  upper margin: %u\n", var->upper_margin);
	dev_dbg(dev, "  lower margin: %u\n", var->lower_margin);
	dev_dbg(dev, "  hsync:        %u\n", var->hsync_len);
	dev_dbg(dev, "  vsync:        %u\n", var->vsync_len);

	if (PICOS2KHZ(var->pixclock) * DIV_ROUND_UP(var->bits_per_pixel, 8)
			> clk_value_khz) {
		dev_err(dev, "%lu KHz pixel clock is too fast\n",
			PICOS2KHZ(var->pixclock));
		return -EINVAL;
	}

	if (var->xres != info->var.xres)
		return -EINVAL;

	if (var->yres != info->var.yres)
		return -EINVAL;

	if (var->xres_virtual != info->var.xres_virtual)
		return -EINVAL;

	if (var->xres_virtual != info->var.xres)
		return -EINVAL;

	if (var->yres_virtual > info->var.yres_virtual)
		return -EINVAL;

	if (var->yres_virtual < info->var.yres)
		return -EINVAL;

	if (info->fix.smem_len) {
		unsigned int smem_len = (var->xres_virtual * var->yres_virtual
					 * DIV_ROUND_UP(var->bits_per_pixel, 8));
		if (smem_len > info->fix.smem_len) {
			dev_err(dev, "Frame buffer too small\n");
			return -EINVAL;
		}
	}

	switch (var->bits_per_pixel) {
	case 1: case 2: case 4: case 8:
		var->red.offset = var->green.offset = var->blue.offset = 0;
		var->red.length = var->green.length = var->blue.length
			= var->bits_per_pixel;
		break;

	case 16:	/* RGB:565 mode */
		var->red.offset		= 0;
		var->green.offset	= 5;
		var->blue.offset	= 11;

		var->red.length		= 5;
		var->green.length	= 6;
		var->blue.length	= 5;
		break;

	case 24:	/* RGB:888 mode */
		var->red.offset		= 0;
		var->green.offset	= 8;
		var->blue.offset	= 16;

		var->red.length = var->green.length = var->blue.length = 8;
		break;

	default:
		dev_err(dev, "color depth %d not supported\n",
					var->bits_per_pixel);
		return -EINVAL;
	}

	return 0;
}

/**
 * ftlcdc100_set_par - Alters the hardware state.
 * @info: frame buffer structure that represents a single frame buffer
 *
 * Using the fb_var_screeninfo in fb_info we set the resolution of the
 * this particular framebuffer. This function alters the par AND the
 * fb_fix_screeninfo stored in fb_info. It does not alter var in 
 * fb_info since we are using that data. This means we depend on the
 * data in var inside fb_info to be supported by the hardware. 
 *
 * This function is also used to recover/restore the hardware to a
 * known working state.
 *
 * fb_check_var is always called before fb_set_par to ensure that
 * the contents of var is always valid.
 *
 * Again if you can't change the resolution you don't need this function.
 *
 * However, even if your hardware does not support mode changing,
 * a set_par might be needed to at least initialize the hardware to
 * a known working state, especially if it came back from another
 * process that also modifies the same hardware, such as X.
 *
 * Returns negative errno on error, or zero on success.
 */
static int ftlcdc100_set_par(struct fb_info *info)
{
	struct ftlcdc100 *ftlcdc100 = info->par;
	unsigned long clk_value_khz;
	unsigned int divno;
	unsigned int reg;

	dev_dbg(info->device, "%s:\n", __func__);
	dev_dbg(info->device, "  resolution:     %ux%u (%ux%u virtual)\n",
		info->var.xres, info->var.yres,
		info->var.xres_virtual, info->var.yres_virtual);

	/*
	 * Fill uninitialized fields of struct fb_fix_screeninfo
	 */
	if (info->var.bits_per_pixel == 1)
		info->fix.visual = FB_VISUAL_MONO01;
	else if (info->var.bits_per_pixel <= 8)
		info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else
		info->fix.visual = FB_VISUAL_TRUECOLOR;

	info->fix.line_length = info->var.xres_virtual *
				  DIV_ROUND_UP(info->var.bits_per_pixel, 8);

	/*
	 * LCD clock and signal polarity control
	 */
	clk_value_khz = LC_CLK / 1000;

	divno = DIV_ROUND_UP(clk_value_khz, PICOS2KHZ(info->var.pixclock));
	if (divno == 0) {
		dev_err(info->device,
			"pixel clock(%lu kHz) > bus clock(%lu kHz)\n",
			PICOS2KHZ(info->var.pixclock), clk_value_khz);
		return -EINVAL;
	}

	clk_value_khz = DIV_ROUND_UP(clk_value_khz, divno);
	info->var.pixclock = KHZ2PICOS(clk_value_khz);
	dev_dbg(info->device, "  updated pixclk: %lu KHz (divno = %x)\n",
		clk_value_khz, divno - 1);

	dev_dbg(info->device, "  frame rate:     %lu Hz\n",
		clk_value_khz * 1000
		/ (info->var.xres + info->var.left_margin
			+ info->var.right_margin + info->var.hsync_len)
		/ (info->var.yres + info->var.upper_margin
			+ info->var.lower_margin + info->var.vsync_len));

	reg = FTLCDC100_LCD_CLOCK_POLARITY_DIVNO(divno - 1)
	    | FTLCDC100_LCD_CLOCK_POLARITY_ADPEN;

	if ((info->var.sync & FB_SYNC_HOR_HIGH_ACT) == 0)
		reg |= FTLCDC100_LCD_CLOCK_POLARITY_IHS;

	if ((info->var.sync & FB_SYNC_VERT_HIGH_ACT) == 0)
		reg |= FTLCDC100_LCD_CLOCK_POLARITY_IVS;

	dev_dbg(info->device, "  [LCD CLOCK POLARITY] = %08x\n", reg);
	iowrite32(reg, ftlcdc100->base + FTLCDC100_OFFSET_LCD_CLOCK_POLARITY);

	/*
	 * LCD horizontal timing control
	 */
	reg = FTLCDC100_LCD_HTIMING_PL(info->var.xres / 16 - 1);
	reg |= FTLCDC100_LCD_HTIMING_HW(info->var.hsync_len - 1);
	reg |= FTLCDC100_LCD_HTIMING_HFP(info->var.right_margin - 1);
	reg |= FTLCDC100_LCD_HTIMING_HBP(info->var.left_margin - 1);

	dev_dbg(info->device, "  [LCD HTIMING] = %08x\n", reg);
	iowrite32(reg, ftlcdc100->base + FTLCDC100_OFFSET_LCD_HTIMING);

	/*
	 * LCD vertical timing control
	 */
	reg = FTLCDC100_LCD_VTIMING_LF(info->var.yres - 1);
	reg |= FTLCDC100_LCD_VTIMING_VW(info->var.vsync_len - 1);
	reg |= FTLCDC100_LCD_VTIMING_VFP(info->var.lower_margin);
	reg |= FTLCDC100_LCD_VTIMING_VBP(info->var.upper_margin);

	dev_dbg(info->device, "  [LCD VTIMING] = %08x\n", reg);
	iowrite32(reg, ftlcdc100->base + FTLCDC100_OFFSET_LCD_VTIMING);

	/*
	 * LCD Panel Pixel Parameters
	 */
	reg = FTLCDC100_LCD_CONTROL_ENABLE
	    | FTLCDC100_LCD_CONTROL_TFT
	    | FTLCDC100_LCD_CONTROL_BGR
	    | FTLCDC100_LCD_CONTROL_LEB_LEP
	    | FTLCDC100_LCD_CONTROL_LCD;

	switch (info->var.bits_per_pixel) {
		case 1:
			reg |= FTLCDC100_LCD_CONTROL_BPP1;
			break;

		case 2:
			reg |= FTLCDC100_LCD_CONTROL_BPP2;
			break;

		case 4:
			reg |= FTLCDC100_LCD_CONTROL_BPP4;
			break;

		case 8:
			reg |= FTLCDC100_LCD_CONTROL_BPP8;
			break;

		case 16:
			reg |= FTLCDC100_LCD_CONTROL_BPP16;
			break;

		case 24:
			reg |= FTLCDC100_LCD_CONTROL_BPP24;
			break;

		default:
			BUG();
			break;
	}

	dev_dbg(info->device, "  [LCD CONTROL] = %08x\n", reg);
	iowrite32(reg, ftlcdc100->base + FTLCDC100_OFFSET_LCD_CONTROL);

	return 0;
}

/**
 * ftlcdc100_pan_display - NOT a required function. Pans the display.
 * @var: frame buffer variable screen structure
 * @info: frame buffer structure that represents a single frame buffer
 *
 * Pan (or wrap, depending on the `vmode' field) the display using the
 * `xoffset' and `yoffset' fields of the `var' structure.
 * If the values don't fit, return -EINVAL.
 *
 * Returns negative errno on error, or zero on success.
 */
static int ftlcdc100_pan_display(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
	struct ftlcdc100 *ftlcdc100 = info->par;
	unsigned long dma_addr;
	unsigned int value;

	dev_dbg(info->device, "%s\n", __func__);

	dma_addr = info->fix.smem_start + var->yoffset * info->fix.line_length;
	value = FTLCDC100_LCD_FRAME_BASE(dma_addr);

	iowrite32(value, ftlcdc100->base + FTLCDC100_OFFSET_LCD_FRAME_BASE);
	dev_dbg(info->device, "  [LCD FRAME BASE] = %08x\n", value);
	return 0;
}

static struct fb_ops ftlcdc100_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= ftlcdc100_check_var,
	.fb_set_par	= ftlcdc100_set_par,
	.fb_pan_display	= ftlcdc100_pan_display,

	/* These are generic software based fb functions */
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

/******************************************************************************
 * struct platform_driver functions
 *****************************************************************************/
static int __init ftlcdc100_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ftlcdc100 *ftlcdc100;
	struct resource *res;
	struct fb_info *info;
	unsigned int reg;
	int irq;
	int ret;

	dev_dbg(dev, "%s\n", __func__);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		return -ENXIO;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto err_get_irq;
	}

	/*
	 * Allocate info and par
	 */
	info = framebuffer_alloc(sizeof(struct ftlcdc100), dev);
	if (!info) {
		dev_err(dev, "Failed to allocate fb_info\n");
		ret = -ENOMEM;
		goto err_alloc_info;
	}

	platform_set_drvdata(pdev, info);

	ftlcdc100 = info->par;

	/*
	 * Set up flags to indicate what sort of acceleration your
	 * driver can provide (pan/wrap/copyarea/etc.) and whether it
	 * is a module -- see FBINFO_* in include/linux/fb.h
	 */
	info->flags = FBINFO_DEFAULT;

	info->fbops = &ftlcdc100_fb_ops;
	info->pseudo_palette = ftlcdc100->pseudo_palette;

	/*
	 * Allocate colormap
	 */
	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to allocate colormap\n");
		goto err_alloc_cmap;
	}

	/*
	 * Map io memory
	 */
	ftlcdc100->res = request_mem_region(res->start, res->end - res->start,
			dev_name(dev));
	if (!ftlcdc100->res) {
		dev_err(dev, "Could not reserve memory region\n");
		ret = -ENOMEM;
		goto err_req_mem_region;
	}

	ftlcdc100->base = ioremap(res->start, res->end - res->start);
	if (!ftlcdc100->base) {
		dev_err(dev, "Failed to ioremap registers\n");
		ret = -EIO;
		goto err_ioremap;
	}

	/*
	 * Copy default parameters
	 */
	info->fix = ftlcdc100_default_fix;
	info->var = ftlcdc100_default_var;

	ret = ftlcdc100_check_var(&info->var, info);
	if (ret < 0) {
		dev_err(dev, "ftlcdc100_check_var() failed\n");
		goto err_check_var;
	}

	/*
	 * Allocate framebuffer
	 */
	info->fix.smem_len = info->var.xres_virtual * info->var.yres_virtual
			     * DIV_ROUND_UP(info->var.bits_per_pixel, 8);

	info->screen_base = dma_alloc_writecombine(NULL, info->fix.smem_len,
				(dma_addr_t *)&info->fix.smem_start,
				GFP_KERNEL | GFP_DMA);

	if (!info->screen_base) {
		dev_err(dev, "Failed to allocate frame buffer\n");
		ret = -ENOMEM;
		goto err_alloc_framebuffer;
	}

	memset(info->screen_base, 0, info->fix.smem_len);
	dev_dbg(dev, "  frame buffer: vitual = %p, physical = %08lx\n",
		info->screen_base, info->fix.smem_start);

	/*
	 * Register interrupt handler
	 */
	ret = request_irq(irq, ftlcdc100_interrupt, IRQF_SHARED, pdev->name, info);
	if (ret < 0) {
		dev_err(dev, "Failed to request irq %d\n", irq);
		goto err_req_irq;
	}

	ftlcdc100->irq = irq;

	/*
	 * Because ftlcdc100_set_par() starts DMA, we need to setup frame base
	 * register before calling it.
	 */
	reg = FTLCDC100_LCD_FRAME_BASE(info->fix.smem_start);
	iowrite32(reg, ftlcdc100->base + FTLCDC100_OFFSET_LCD_FRAME_BASE);

	/*
	 * Enable interrupts
	 */
	reg = FTLCDC100_LCD_INT_UNDERRUN
	    | FTLCDC100_LCD_INT_BUS_ERROR;

	iowrite32(reg, ftlcdc100->base + FTLCDC100_OFFSET_LCD_INT_ENABLE);

	/*
	 * Does a call to fb_set_par() before register_framebuffer needed?  This
	 * will depend on you and the hardware.  If you are sure that your driver
	 * is the only device in the system, a call to fb_set_par() is safe.
	 *
	 * Hardware in x86 systems has a VGA core.  Calling set_par() at this
	 * point will corrupt the VGA console, so it might be safer to skip a
	 * call to set_par here and just allow fbcon to do it for you.
	 */
	ftlcdc100_set_par(info);

	/*
	 * Tell the world that we're ready to go
	 */
	if (register_framebuffer(info) < 0) {
		dev_err(dev, "Failed to register frame buffer\n");
		ret = -EINVAL;
		goto err_register_info;
	}

	dev_info(dev, "fb%d: %s frame buffer device\n", info->node,
		info->fix.id);
	return 0;

err_register_info:
	/* disable LCD HW */
	iowrite32(0, ftlcdc100->base + FTLCDC100_OFFSET_LCD_CONTROL);
	free_irq(irq, info);
err_req_irq:
	dma_free_writecombine(NULL, info->fix.smem_len, info->screen_base,
				(dma_addr_t )info->fix.smem_start);
err_alloc_framebuffer:
err_check_var:
	iounmap(ftlcdc100->base);
err_ioremap:
err_req_mem_region:
	fb_dealloc_cmap(&info->cmap);
err_alloc_cmap:
	platform_set_drvdata(pdev, NULL);
	framebuffer_release(info);
err_alloc_info:
	release_resource(res);
err_get_irq:
	return ret;
}

static int __exit ftlcdc100_remove(struct platform_device *pdev)
{
	struct fb_info *info;
	struct ftlcdc100 *ftlcdc100;

	info = platform_get_drvdata(pdev);
	ftlcdc100 = info->par;

	/* disable LCD HW */
	iowrite32(0, ftlcdc100->base + FTLCDC100_OFFSET_LCD_INT_ENABLE);
	iowrite32(0, ftlcdc100->base + FTLCDC100_OFFSET_LCD_CONTROL);

	unregister_framebuffer(info);
	free_irq(ftlcdc100->irq, info);

	dma_free_writecombine(NULL, info->fix.smem_len, info->screen_base,
				(dma_addr_t )info->fix.smem_start);

	iounmap(ftlcdc100->base);

	fb_dealloc_cmap(&info->cmap);
	platform_set_drvdata(pdev, NULL);
	framebuffer_release(info);
	release_resource(ftlcdc100->res);

	return 0;
}

static struct platform_driver ftlcdc100_driver = {
	.probe		= ftlcdc100_probe,
	.remove		= __exit_p(ftlcdc100_remove),

	.driver		= {
		.name	= "ftlcdc100",
		.owner	= THIS_MODULE,
	},
};

/******************************************************************************
 * initialization / finalization
 *****************************************************************************/
static int __init ftlcdc100_init(void)
{
	return platform_driver_register(&ftlcdc100_driver);
}

static void __exit ftlcdc100_exit(void)
{
	platform_driver_unregister(&ftlcdc100_driver);
}

module_init(ftlcdc100_init);
module_exit(ftlcdc100_exit);

MODULE_DESCRIPTION("FTLCDC100 LCD Controller framebuffer driver");
MODULE_AUTHOR("Po-Yu Chuang <ratbert@faraday-tech.com>");
MODULE_LICENSE("GPL");
