#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/unaligned.h>

#define MPU6050_REG_SMPLRT_DIV	0x19
#define MPU6050_REG_CONFIG	0x1a
#define MPU6050_REG_GYRO_CONFIG	0x1b
#define MPU6050_REG_ACCEL_CONFIG	0x1c
#define MPU6050_REG_INT_PIN_CFG	0x37
#define MPU6050_REG_INT_ENABLE	0x38
#define MPU6050_REG_INT_STATUS	0x3a
#define MPU6050_REG_ACCEL_XOUT_H	0x3b
#define MPU6050_REG_GYRO_XOUT_H	0x43
#define MPU6050_REG_PWR_MGMT_1	0x6b
#define MPU6050_REG_PWR_MGMT_2	0x6c
#define MPU6050_REG_WHO_AM_I	0x75

#define MPU6050_WHO_AM_I		0x68
#define MPU6050_RESET		0x80
#define MPU6050_SLEEP		0x40
#define MPU6050_CLK_PLL_XGYRO	0x01
#define MPU6050_STARTUP_MS	100
#define MPU6050_INT_LATCH		0x20
#define MPU6050_DATA_READY	0x01

/* 9.80665 / 16384, (pi / 180) / 131을 소수점 9자리로 나타낸다. */
#define MPU6050_ACCEL_SCALE_NANO	598550
#define MPU6050_GYRO_SCALE_NANO	133231

struct mpu6050_data {
	struct i2c_client *client;
	struct iio_trigger *trigger;
};

#define MPU6050_CHANNEL(_type, _axis, _reg, _index) { \
	.type = _type, \
	.modified = 1, \
	.channel2 = _axis, \
	.address = _reg, \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
	.scan_index = _index, \
	.scan_type = { \
		.sign = 's', \
		.realbits = 16, \
		.storagebits = 16, \
		.endianness = IIO_BE, \
	}, \
}

static const struct iio_chan_spec mpu6050_channels[] = {
	MPU6050_CHANNEL(IIO_ACCEL, IIO_MOD_X, MPU6050_REG_ACCEL_XOUT_H, 0),
	MPU6050_CHANNEL(IIO_ACCEL, IIO_MOD_Y, MPU6050_REG_ACCEL_XOUT_H + 2, 1),
	MPU6050_CHANNEL(IIO_ACCEL, IIO_MOD_Z, MPU6050_REG_ACCEL_XOUT_H + 4, 2),
	MPU6050_CHANNEL(IIO_ANGL_VEL, IIO_MOD_X, MPU6050_REG_GYRO_XOUT_H, 3),
	MPU6050_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Y, MPU6050_REG_GYRO_XOUT_H + 2, 4),
	MPU6050_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Z, MPU6050_REG_GYRO_XOUT_H + 4, 5),
	IIO_CHAN_SOFT_TIMESTAMP(6),
};

/* 센서 읽기는 항상 6축이다. 사용자 선택 채널의 추출은 IIO가 맡는다. */
static const unsigned long mpu6050_scan_masks[] = { GENMASK(5, 0), 0 };

static int mpu6050_read_raw(struct iio_dev *indio_dev,
			  const struct iio_chan_spec *chan,
			  int *val, int *val2, long mask)
{
	struct mpu6050_data *data = iio_priv(indio_dev);
	u8 raw[2];
	int ret;

	if (mask == IIO_CHAN_INFO_SCALE) {
		*val = 0;
		switch (chan->type) {
		case IIO_ACCEL:
			*val2 = MPU6050_ACCEL_SCALE_NANO;
			break;
		case IIO_ANGL_VEL:
			*val2 = MPU6050_GYRO_SCALE_NANO;
			break;
		default:
			return -EINVAL;
		}
		return IIO_VAL_INT_PLUS_NANO;
	}

	if (mask != IIO_CHAN_INFO_RAW)
		return -EINVAL;
	if (!iio_device_claim_direct(indio_dev))
		return -EBUSY;

	ret = i2c_smbus_read_i2c_block_data(data->client, chan->address,
					 sizeof(raw), raw);
	if (ret < 0)
		goto out;
	if (ret != sizeof(raw)) {
		ret = -EIO;
		goto out;
	}

	*val = (s16)get_unaligned_be16(raw);
	ret = IIO_VAL_INT;

out:
	iio_device_release_direct(indio_dev);
	return ret;
}

static const struct iio_info mpu6050_info = {
	.read_raw = mpu6050_read_raw,
};

static int mpu6050_set_trigger_state(struct iio_trigger *trigger, bool enable)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trigger);
	struct mpu6050_data *data = iio_priv(indio_dev);
	struct i2c_client *client = data->client;
	int ret;

	if (!enable) {
		/* 수집 콜백이 끝난 뒤 센서 알림을 끈다. I2C 실패도 해제를 막지 않는다. */
		disable_irq(client->irq);
		ret = i2c_smbus_write_byte_data(client, MPU6050_REG_INT_ENABLE, 0x00);
		if (ret < 0)
			dev_warn(&client->dev, "[BUFFER] failed to disable sensor IRQ: %d\n", ret);
		dev_info(&client->dev, "[BUFFER] disabled\n");
		return 0;
	}

	ret = i2c_smbus_read_byte_data(client, MPU6050_REG_INT_STATUS);
	if (ret < 0)
		return ret;
	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_INT_ENABLE,
				       MPU6050_DATA_READY);
	if (ret < 0)
		return ret;

	enable_irq(client->irq);
	dev_info(&client->dev, "[BUFFER] enabled: 100 Hz nominal\n");
	return 0;
}

static const struct iio_trigger_ops mpu6050_trigger_ops = {
	.set_trigger_state = mpu6050_set_trigger_state,
	.validate_device = iio_trigger_validate_own_device,
};

static irqreturn_t mpu6050_irq(int irq, void *private)
{
	struct iio_dev *indio_dev = private;

	/* I2C 전송 없이 IRQ 관측 시각만 기록하고 스레드를 깨운다. */
	indio_dev->pollfunc->timestamp = iio_get_time_ns(indio_dev);
	return IRQ_WAKE_THREAD;
}

static irqreturn_t mpu6050_irq_thread(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct mpu6050_data *data = iio_priv(indio_dev);
	int status;

	/* 래치된 INT를 해제한 뒤 IIO의 수집 콜백을 호출한다. */
	status = i2c_smbus_read_byte_data(data->client, MPU6050_REG_INT_STATUS);
	if (status < 0)
		dev_err_ratelimited(&data->client->dev, "[IRQ] status read failed: %d\n", status);
	else if (status & MPU6050_DATA_READY)
		iio_trigger_poll_nested(data->trigger);

	return IRQ_HANDLED;
}

static irqreturn_t mpu6050_trigger_handler(int irq, void *private)
{
	struct iio_poll_func *pf = private;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct mpu6050_data *data = iio_priv(indio_dev);
	u8 raw[14];
	IIO_DECLARE_BUFFER_WITH_TS(__be16, scan, 6) = { };
	int ret;

	ret = i2c_smbus_read_i2c_block_data(data->client, MPU6050_REG_ACCEL_XOUT_H,
					 sizeof(raw), raw);
	if (ret < 0 || ret != sizeof(raw)) {
		dev_err_ratelimited(&data->client->dev, "[BUFFER] sample read failed: %d\n",
				    ret < 0 ? ret : -EIO);
		goto out;
	}

	/* 온도 두 바이트를 제외하고 센서의 big-endian 형식을 그대로 전달한다. */
	memcpy(scan, raw, 6);
	memcpy(&scan[3], &raw[8], 6);
	ret = iio_push_to_buffers_with_ts(indio_dev, scan, sizeof(scan), pf->timestamp);
	if (ret < 0)
		dev_warn_ratelimited(&data->client->dev, "[BUFFER] sample dropped: %d\n", ret);

out:
	/* 오류로 샘플을 버린 경우에도 다음 트리거를 받을 수 있게 완료를 알린다. */
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int mpu6050_write_verify(struct i2c_client *client, u8 reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return ret;
	if (ret != value)
		return -EIO;

	return 0;
}

static int mpu6050_init_sensor(struct i2c_client *client)
{
	int ret;

	/* 이전 FIFO·IRQ 설정을 지우고 버퍼를 켤 때만 Data Ready를 활성화한다. */
	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_PWR_MGMT_1,
				       MPU6050_RESET);
	if (ret < 0)
		return ret;

	msleep(MPU6050_STARTUP_MS);

	ret = mpu6050_write_verify(client, MPU6050_REG_PWR_MGMT_1,
				   MPU6050_CLK_PLL_XGYRO);
	if (ret)
		return ret;

	/* 슬립 해제 후 센서와 클럭이 기동할 시간을 확보한다. */
	msleep(MPU6050_STARTUP_MS);

	ret = mpu6050_write_verify(client, MPU6050_REG_PWR_MGMT_2, 0x00);
	if (ret)
		return ret;

	/* 기존 실습과 같은 ±250 deg/s, ±2 g 범위다. */
	ret = mpu6050_write_verify(client, MPU6050_REG_GYRO_CONFIG, 0x00);
	if (ret)
		return ret;
	ret = mpu6050_write_verify(client, MPU6050_REG_ACCEL_CONFIG, 0x00);
	if (ret)
		return ret;

	/* 센서 내부 갱신은 공칭 100 Hz이며 sysfs 조회 주기와는 다르다. */
	ret = mpu6050_write_verify(client, MPU6050_REG_CONFIG, 0x03);
	if (ret)
		return ret;
	ret = mpu6050_write_verify(client, MPU6050_REG_SMPLRT_DIV, 9);
	if (ret)
		return ret;
	return mpu6050_write_verify(client, MPU6050_REG_INT_PIN_CFG, MPU6050_INT_LATCH);
}

static void mpu6050_sleep(struct i2c_client *client)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_INT_ENABLE, 0x00);
	if (ret < 0)
		dev_warn(&client->dev, "[POWER] failed to disable sensor IRQ: %d\n", ret);
	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_PWR_MGMT_1,
				       MPU6050_SLEEP);
	if (ret < 0)
		dev_warn(&client->dev, "[POWER] failed to enter sleep: %d\n", ret);
}

static int mpu6050_probe(struct i2c_client *client)
{
	struct iio_dev *indio_dev;
	struct mpu6050_data *data;
	int ret;

	if (client->irq <= 0)
		return dev_err_probe(&client->dev, -EINVAL, "[PROBE] missing DT interrupt\n");

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		return dev_err_probe(&client->dev, -EOPNOTSUPP,
				     "[PROBE] required I2C transfers unsupported\n");

	ret = i2c_smbus_read_byte_data(client, MPU6050_REG_WHO_AM_I);
	if (ret < 0)
		return dev_err_probe(&client->dev, ret, "[PROBE] WHO_AM_I read failed\n");
	if (ret != MPU6050_WHO_AM_I)
		return dev_err_probe(&client->dev, -ENODEV,
				     "[PROBE] unexpected WHO_AM_I=0x%02x\n", ret);

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->client = client;
	indio_dev->name = "mpu6050-iio-study";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &mpu6050_info;
	indio_dev->channels = mpu6050_channels;
	indio_dev->num_channels = ARRAY_SIZE(mpu6050_channels);
	indio_dev->available_scan_masks = mpu6050_scan_masks;
	i2c_set_clientdata(client, indio_dev);

	ret = iio_device_set_clock(indio_dev, CLOCK_MONOTONIC);
	if (ret)
		return dev_err_probe(&client->dev, ret, "[PROBE] timestamp clock setup failed\n");

	ret = mpu6050_init_sensor(client);
	if (ret)
		goto err_sleep;

	ret = iio_triggered_buffer_setup(indio_dev, NULL, mpu6050_trigger_handler, NULL);
	if (ret)
		goto err_sleep;

	data->trigger = devm_iio_trigger_alloc(&client->dev, "%s-%s",
					     indio_dev->name, dev_name(&client->dev));
	if (!data->trigger) {
		ret = -ENOMEM;
		goto err_buffer;
	}
	data->trigger->ops = &mpu6050_trigger_ops;
	iio_trigger_set_drvdata(data->trigger, indio_dev);
	ret = iio_trigger_register(data->trigger);
	if (ret)
		goto err_buffer;

	ret = iio_trigger_set_immutable(indio_dev, data->trigger);
	if (ret)
		goto err_trigger;

	/* 등록 시에는 IRQ를 막아 두고 버퍼 활성화 시 허용한다. */
	ret = request_threaded_irq(client->irq, mpu6050_irq, mpu6050_irq_thread,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQF_NO_AUTOEN,
				   "mpu6050-iio-data-ready", indio_dev);
	if (ret)
		goto err_trigger;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto err_irq;

	dev_info(&client->dev, "[IIO] ready: raw + scale + IRQ timestamp buffer, scan=24 bytes\n");
	return 0;

err_irq:
	free_irq(client->irq, indio_dev);
err_trigger:
	iio_trigger_unregister(data->trigger);
err_buffer:
	iio_triggered_buffer_cleanup(indio_dev);
err_sleep:
	mpu6050_sleep(client);
	return dev_err_probe(&client->dev, ret, "[PROBE] IIO setup failed\n");
}

static void mpu6050_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct mpu6050_data *data = iio_priv(indio_dev);

	/* IIO가 버퍼를 끄고 수집 콜백을 분리한 뒤 자원을 역순으로 정리한다. */
	iio_device_unregister(indio_dev);
	free_irq(client->irq, indio_dev);
	iio_trigger_unregister(data->trigger);
	iio_triggered_buffer_cleanup(indio_dev);
	mpu6050_sleep(client);
	dev_info(&client->dev, "[REMOVE] detached\n");
}

static const struct of_device_id mpu6050_match[] = {
	{ .compatible = "nakta,mpu6050-iio-study" },
	{},
};
MODULE_DEVICE_TABLE(of, mpu6050_match);

static const struct i2c_device_id mpu6050_ids[] = {
	{ "mpu6050-iio-study", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mpu6050_ids);

static struct i2c_driver mpu6050_driver = {
	.probe = mpu6050_probe,
	.remove = mpu6050_remove,
	.id_table = mpu6050_ids,
	.driver = {
		.name = "mpu6050-iio-study",
		.of_match_table = mpu6050_match,
	},
};
module_i2c_driver(mpu6050_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nakta");
MODULE_DESCRIPTION("MPU6050 IIO raw, scale and triggered buffer practice");
