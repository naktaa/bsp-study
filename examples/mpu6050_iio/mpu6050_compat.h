/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MPU6050_COMPAT_H
#define MPU6050_COMPAT_H

#include <linux/align.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
#include <asm/unaligned.h>
#else
#include <linux/unaligned.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 15, 0)
static inline bool iio_device_claim_direct(struct iio_dev *indio_dev)
{
	return iio_device_claim_direct_mode(indio_dev) == 0;
}

static inline void iio_device_release_direct(struct iio_dev *indio_dev)
{
	iio_device_release_direct_mode(indio_dev);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 16, 0)
#define IIO_DECLARE_BUFFER_WITH_TS(type, name, count) \
	type name[ALIGN((count), sizeof(s64) / sizeof(type)) + \
		  sizeof(s64) / sizeof(type)] __aligned(8)

/* 구 API는 버퍼 크기를 받지 않으므로 선언부에서 timestamp 공간을 확보한다. */
static inline int iio_push_to_buffers_with_ts(struct iio_dev *indio_dev,
					    void *data, size_t data_total_len,
					    s64 timestamp)
{
	return iio_push_to_buffers_with_timestamp(indio_dev, data, timestamp);
}
#endif

#endif /* MPU6050_COMPAT_H */
