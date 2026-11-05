#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/unaligned.h>
#include <linux/wait.h>

#include "mpu6050_data.h"

#define MPU6050_REG_SMPLRT_DIV	0x19
#define MPU6050_REG_CONFIG	0x1a
#define MPU6050_REG_GYRO_CONFIG	0x1b
#define MPU6050_REG_ACCEL_CONFIG	0x1c
#define MPU6050_REG_INT_PIN_CFG	0x37
#define MPU6050_REG_INT_ENABLE	0x38
#define MPU6050_REG_INT_STATUS	0x3a
#define MPU6050_REG_ACCEL_XOUT_H	0x3b
#define MPU6050_REG_PWR_MGMT_1	0x6b
#define MPU6050_REG_PWR_MGMT_2	0x6c
#define MPU6050_REG_WHO_AM_I	0x75

#define MPU6050_WHO_AM_I		0x68
#define MPU6050_RESET		0x80
#define MPU6050_SLEEP		0x40
#define MPU6050_CLK_PLL_XGYRO	0x01
#define MPU6050_GYRO_FS_250	0x00
#define MPU6050_ACCEL_FS_2G	0x00
#define MPU6050_STARTUP_MS	100
#define MPU6050_SAMPLE_BYTES	14
#define MPU6050_DLPF_CFG_3	0x03
#define MPU6050_DIV_100HZ		9
#define MPU6050_INT_LATCH		0x20
#define MPU6050_DATA_READY	0x01

struct mpu6050_state {
	struct i2c_client *client;
	struct miscdevice misc;
	struct mutex lock;
	struct kref ref;
	unsigned long irq_count;
	wait_queue_head_t wait;
	struct mpu6050_sample latest;
	u32 sequence;
	int sample_error;
};

struct mpu6050_file {
	struct mpu6050_state *state;
	struct mutex read_lock;
	u32 seen_sequence;
};

static int mpu6050_write_verify(struct i2c_client *client, u8 reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		return dev_err_probe(&client->dev, ret,
				     "[INIT] failed to write reg=0x%02x\n", reg);

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return dev_err_probe(&client->dev, ret,
				     "[INIT] failed to read reg=0x%02x\n", reg);

	if (ret != value)
		return dev_err_probe(&client->dev, -EIO,
				     "[INIT] reg=0x%02x expected=0x%02x, got=0x%02x\n",
				     reg, value, ret);

	dev_info(&client->dev, "[INIT] reg=0x%02x, value=0x%02x\n", reg, ret);
	return 0;
}

static int mpu6050_init_sensor(struct i2c_client *client)
{
	int ret;

	/* 이전 실행의 설정을 지운다. 리셋 비트는 자동으로 해제된다. */
	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_PWR_MGMT_1,
				       MPU6050_RESET);
	if (ret < 0)
		return dev_err_probe(&client->dev, ret,
				     "[INIT] failed to reset sensor\n");

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

	ret = mpu6050_write_verify(client, MPU6050_REG_GYRO_CONFIG,
				   MPU6050_GYRO_FS_250);
	if (ret)
		return ret;

	ret = mpu6050_write_verify(client, MPU6050_REG_ACCEL_CONFIG,
				   MPU6050_ACCEL_FS_2G);
	if (ret)
		return ret;

	/* DLPF 활성 시 1kHz를 10분주해 Data Ready의 공칭 주기를 10ms로 정한다. */
	ret = mpu6050_write_verify(client, MPU6050_REG_CONFIG, MPU6050_DLPF_CFG_3);
	if (ret)
		return ret;

	ret = mpu6050_write_verify(client, MPU6050_REG_SMPLRT_DIV, MPU6050_DIV_100HZ);
	if (ret)
		return ret;

	/* INT를 HIGH로 유지하고 INT_STATUS를 읽을 때만 해제한다. */
	ret = mpu6050_write_verify(client, MPU6050_REG_INT_PIN_CFG, MPU6050_INT_LATCH);
	if (ret)
		return ret;

	dev_info(&client->dev, "[INIT] ready: accel range=+/-2 g, gyro range=+/-250 deg/s\n");
	return 0;
}

static int mpu6050_read_sample(struct i2c_client *client,
			       struct mpu6050_sample *sample)
{
	u8 data[MPU6050_SAMPLE_BYTES];
	int ret;

	/* 같은 시점의 축 데이터를 얻도록 온도를 포함한 구간을 한 번에 읽는다. */
	ret = i2c_smbus_read_i2c_block_data(client, MPU6050_REG_ACCEL_XOUT_H,
					   sizeof(data), data);
	if (ret < 0)
		return ret;
	if (ret != MPU6050_SAMPLE_BYTES)
		return -EIO;

	sample->accel[0] = (s16)get_unaligned_be16(&data[0]);
	sample->accel[1] = (s16)get_unaligned_be16(&data[2]);
	sample->accel[2] = (s16)get_unaligned_be16(&data[4]);

	/* 바이트 6~7은 온도 값이며 이번에는 출력하지 않는다. */
	sample->gyro[0] = (s16)get_unaligned_be16(&data[8]);
	sample->gyro[1] = (s16)get_unaligned_be16(&data[10]);
	sample->gyro[2] = (s16)get_unaligned_be16(&data[12]);
	return 0;
}

static void mpu6050_free(struct kref *ref)
{
	struct mpu6050_state *state = container_of(ref, struct mpu6050_state, ref);

	kfree(state);
}

static int mpu6050_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *misc = filp->private_data;
	struct mpu6050_state *state = container_of(misc, struct mpu6050_state, misc);
	struct mpu6050_file *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	/* misc 코어가 open과 deregister를 직렬화하는 동안 참조를 확보한다. */
	kref_get(&state->ref);
	ctx->state = state;
	mutex_init(&ctx->read_lock);

	mutex_lock(&state->lock);
	ctx->seen_sequence = state->sequence;
	mutex_unlock(&state->lock);

	filp->private_data = ctx;
	return nonseekable_open(inode, filp);
}

static int mpu6050_release(struct inode *inode, struct file *filp)
{
	struct mpu6050_file *ctx = filp->private_data;

	kref_put(&ctx->state->ref, mpu6050_free);
	kfree(ctx);
	return 0;
}

static ssize_t mpu6050_read(struct file *filp, char __user *buf,
			    size_t count, loff_t *f_pos)
{
	struct mpu6050_file *ctx = filp->private_data;
	struct mpu6050_state *state = ctx->state;
	struct mpu6050_sample sample;
	int ret;

	if (!count)
		return 0;
	if (count < sizeof(sample))
		return -EINVAL;

	/* 같은 열린 파일의 동시 read가 같은 순번을 중복 전달하지 않게 한다. */
	ret = mutex_lock_interruptible(&ctx->read_lock);
	if (ret)
		return ret;

	for (;;) {
		ret = mutex_lock_interruptible(&state->lock);
		if (ret)
			goto out;

		if (!state->client) {
			mutex_unlock(&state->lock);
			ret = -ENODEV;
			goto out;
		}

		if (state->sequence != ctx->seen_sequence) {
			ret = state->sample_error;
			if (ret)
				ctx->seen_sequence = state->sequence;
			else
				sample = state->latest;
			mutex_unlock(&state->lock);
			break;
		}

		mutex_unlock(&state->lock);

		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto out;
		}

		/* 상태 mutex를 놓고 자야 IRQ 스레드가 새 샘플을 저장할 수 있다. */
		ret = wait_event_interruptible(state->wait,
			!READ_ONCE(state->client) ||
			READ_ONCE(state->sequence) != ctx->seen_sequence);
		if (ret)
			goto out;
	}

	if (ret)
		goto out;
	if (copy_to_user(buf, &sample, sizeof(sample))) {
		ret = -EFAULT;
		goto out;
	}

	ctx->seen_sequence = sample.sequence;
	ret = sizeof(sample);

out:
	mutex_unlock(&ctx->read_lock);
	return ret;
}

static const struct file_operations mpu6050_fops = {
	.owner = THIS_MODULE,
	.open = mpu6050_open,
	.release = mpu6050_release,
	.read = mpu6050_read,
};

static void mpu6050_sleep(struct i2c_client *client)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_PWR_MGMT_1,
				       MPU6050_SLEEP);
	if (ret < 0)
		dev_warn(&client->dev, "[POWER] failed to enter sleep: %d\n", ret);
}

static irqreturn_t mpu6050_irq_thread(int irq, void *data)
{
	struct mpu6050_state *state = data;
	struct mpu6050_sample sample;
	bool notify = false;
	u32 sequence;
	int status;
	int ret;

	mutex_lock(&state->lock);

	if (!state->client)
		goto out;

	status = i2c_smbus_read_byte_data(state->client, MPU6050_REG_INT_STATUS);
	if (status < 0) {
		ret = status;
	} else if (status & MPU6050_DATA_READY) {
		state->irq_count++;
		ret = mpu6050_read_sample(state->client, &sample);
	} else {
		goto out;
	}

	/* 최신 한 건만 보관하므로 느린 독자가 읽기 전 값은 덮어써진다. */
	sequence = state->sequence + 1;
	if (!ret) {
		sample.sequence = sequence;
		state->latest = sample;
	} else {
		dev_err_ratelimited(&state->client->dev,
				    "[IRQ] sample acquisition failed: %d\n", ret);
	}
	state->sample_error = ret;
	WRITE_ONCE(state->sequence, sequence);
	notify = true;

out:
	mutex_unlock(&state->lock);
	if (notify)
		wake_up_interruptible_all(&state->wait);
	return IRQ_HANDLED;
}

static void mpu6050_stop_irq(struct mpu6050_state *state)
{
	struct i2c_client *client;
	int ret;

	mutex_lock(&state->lock);

	client = state->client;
	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_INT_ENABLE, 0x00);
	if (ret < 0)
		dev_warn(&client->dev, "[IRQ] failed to disable source: %d\n", ret);
	WRITE_ONCE(state->client, NULL);

	mutex_unlock(&state->lock);
	wake_up_interruptible_all(&state->wait);

	/* 핸들러도 같은 mutex를 쓰므로 잠금을 푼 뒤 핸들러 종료를 기다린다. */
	free_irq(client->irq, state);
}

static int mpu6050_probe(struct i2c_client *client)
{
	struct mpu6050_state *state;
	int ret;

	if (client->irq <= 0)
		return dev_err_probe(&client->dev, -EINVAL,
				     "[IRQ] missing Device Tree interrupt\n");

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		return dev_err_probe(&client->dev, -EOPNOTSUPP,
				     "[PROBE] required I2C transfers unsupported\n");

	ret = i2c_smbus_read_byte_data(client, MPU6050_REG_WHO_AM_I);
	if (ret < 0)
		return dev_err_probe(&client->dev, ret,
				     "[PROBE] failed to read WHO_AM_I\n");

	if (ret != MPU6050_WHO_AM_I)
		return dev_err_probe(&client->dev, -ENODEV,
				     "[PROBE] unexpected WHO_AM_I=0x%02x\n",
				     ret);

	dev_info(&client->dev, "[PROBE] addr=0x%02x, WHO_AM_I=0x%02x\n",
		 client->addr, ret);

	ret = mpu6050_init_sensor(client);
	if (ret)
		goto err_sleep;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state) {
		ret = -ENOMEM;
		goto err_sleep;
	}

	state->client = client;
	mutex_init(&state->lock);
	init_waitqueue_head(&state->wait);
	kref_init(&state->ref);
	state->misc.minor = MISC_DYNAMIC_MINOR;
	state->misc.name = "mpu6050";
	state->misc.fops = &mpu6050_fops;
	state->misc.parent = &client->dev;
	state->misc.mode = 0600;

	ret = i2c_smbus_read_byte_data(client, MPU6050_REG_INT_STATUS);
	if (ret < 0) {
		dev_err_probe(&client->dev, ret, "[IRQ] failed to clear pending status\n");
		goto err_put;
	}

	ret = request_threaded_irq(client->irq, NULL, mpu6050_irq_thread,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "mpu6050-data-ready", state);
	if (ret) {
		dev_err_probe(&client->dev, ret, "[IRQ] registration failed\n");
		goto err_put;
	}

	/* Linux의 수신 준비가 끝난 뒤 센서의 알림을 활성화한다. */
	ret = mpu6050_write_verify(client, MPU6050_REG_INT_ENABLE, MPU6050_DATA_READY);
	if (ret)
		goto err_irq;

	/* 모든 상태를 준비한 뒤 사용자에게 장치 노드를 공개한다. */
	ret = misc_register(&state->misc);
	if (ret) {
		dev_err_probe(&client->dev, ret, "[CHAR] registration failed\n");
		goto err_irq;
	}

	i2c_set_clientdata(client, state);
	dev_info(&client->dev, "[IRQ] irq=%d, data-ready nominal=100 Hz\n", client->irq);
	dev_info(&client->dev, "[CHAR] /dev/mpu6050 ready: IRQ latest-sample read\n");

	return 0;

err_irq:
	mpu6050_stop_irq(state);
err_put:
	kref_put(&state->ref, mpu6050_free);
err_sleep:
	/* 초기화·등록 실패 원인은 유지하고 슬립 전환은 별도로 시도한다. */
	mpu6050_sleep(client);
	return ret;
}

static void mpu6050_remove(struct i2c_client *client)
{
	struct mpu6050_state *state = i2c_get_clientdata(client);

	/* 새 open을 막고 진행 중인 읽기가 끝난 뒤 센서 접근을 차단한다. */
	misc_deregister(&state->misc);
	mpu6050_stop_irq(state);
	mpu6050_sleep(client);

	dev_info(&client->dev, "[REMOVE] detached, data-ready count=%lu\n", state->irq_count);
	/* 열린 파일이 남아 있으면 마지막 release에서 메모리를 해제한다. */
	kref_put(&state->ref, mpu6050_free);
}

/* 기존 IIO 드라이버와 구분하는 학습용 식별자다. */
static const struct of_device_id mpu6050_match[] = {
	{ .compatible = "nakta,mpu6050-study" },
	{},
};
MODULE_DEVICE_TABLE(of, mpu6050_match);

static const struct i2c_device_id mpu6050_ids[] = {
	{ "mpu6050-study", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mpu6050_ids);

static struct i2c_driver mpu6050_driver = {
	.probe = mpu6050_probe,
	.remove = mpu6050_remove,
	.id_table = mpu6050_ids,
	.driver = {
		.name = "mpu6050-study",
		.of_match_table = mpu6050_match,
	},
};
module_i2c_driver(mpu6050_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nakta");
MODULE_DESCRIPTION("MPU6050 I2C raw sample character interface practice");
