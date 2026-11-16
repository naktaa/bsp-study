/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef IMX219_COMPAT_H
#define IMX219_COMPAT_H

#include <linux/clk.h>
#include <linux/version.h>

#include <media/v4l2-common.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
static inline struct clk *
devm_v4l2_sensor_clk_get(struct device *dev, const char *id)
{
	return devm_clk_get(dev, id);
}
#endif

#endif /* IMX219_COMPAT_H */
