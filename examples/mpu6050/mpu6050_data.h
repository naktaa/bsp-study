#ifndef MPU6050_DATA_H
#define MPU6050_DATA_H

#include <linux/types.h>

/* 수집 결과 순번과 raw 한 샘플이며 각 배열은 X, Y, Z 순서다. */
struct mpu6050_sample {
	__u32 sequence;
	__s16 accel[3];
	__s16 gyro[3];
};

#endif
