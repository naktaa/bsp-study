#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
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
#define MPU6050_REG_FIFO_EN	0x23
#define MPU6050_REG_INT_PIN_CFG	0x37
#define MPU6050_REG_INT_ENABLE	0x38
#define MPU6050_REG_INT_STATUS	0x3a
#define MPU6050_REG_USER_CTRL	0x6a
#define MPU6050_REG_PWR_MGMT_1	0x6b
#define MPU6050_REG_PWR_MGMT_2	0x6c
#define MPU6050_REG_FIFO_COUNT_H	0x72
#define MPU6050_REG_FIFO_R_W	0x74
#define MPU6050_REG_WHO_AM_I	0x75

#define MPU6050_WHO_AM_I		0x68
#define MPU6050_RESET		0x80
#define MPU6050_SLEEP		0x40
#define MPU6050_CLK_PLL_XGYRO	0x01
#define MPU6050_GYRO_FS_250	0x00
#define MPU6050_ACCEL_FS_2G	0x00
#define MPU6050_STARTUP_MS	100
#define MPU6050_PACKET_BYTES	12
#define MPU6050_FIFO_BYTES	1024
#define MPU6050_FIFO_CHUNK	24
#define MPU6050_QUEUE_SAMPLES	128
#define MPU6050_FIFO_SENSORS	0x78
#define MPU6050_FIFO_ENABLE	0x40
#define MPU6050_FIFO_RESET	0x04
#define MPU6050_FIFO_OVERFLOW	0x10
#define MPU6050_DLPF_CFG_3	0x03
#define MPU6050_DIV_100HZ		9
#define MPU6050_INT_LATCH		0x20
#define MPU6050_DATA_READY	0x01
#define MPU6050_IRQ_ENABLE	(MPU6050_DATA_READY | MPU6050_FIFO_OVERFLOW)

struct mpu6050_state {
	struct i2c_client *client;
	struct miscdevice misc;
	struct mutex lock;
	struct kref ref;
	unsigned long irq_count;
	wait_queue_head_t wait;
	struct mpu6050_sample queue[MPU6050_QUEUE_SAMPLES];
	u32 sequence;
	unsigned int head;
	unsigned int queued;
	bool opened;
	int error;
	int fatal_error;
	unsigned long fifo_overflows;
	unsigned long fifo_resets;
	unsigned long queue_overruns;
	unsigned int max_batch;
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

static int mpu6050_reset_fifo(struct i2c_client *client)
{
	int value;
	int ret;

	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_FIFO_EN, 0x00);
	if (ret < 0)
		return ret;
	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_USER_CTRL, 0x00);
	if (ret < 0)
		return ret;

	/* FIFO 동작을 끈 상태에서만 리셋하고 자동 해제를 기다린다. */
	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_USER_CTRL, MPU6050_FIFO_RESET);
	if (ret < 0)
		return ret;
	ret = read_poll_timeout(i2c_smbus_read_byte_data, value,
				value < 0 || !(value & MPU6050_FIFO_RESET),
				1000, 10000, false, client, MPU6050_REG_USER_CTRL);
	if (ret)
		return ret;
	if (value < 0)
		return value;

	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_USER_CTRL, MPU6050_FIFO_ENABLE);
	if (ret < 0)
		return ret;
	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_FIFO_EN, MPU6050_FIFO_SENSORS);
	if (ret < 0)
		return ret;

	value = i2c_smbus_read_byte_data(client, MPU6050_REG_USER_CTRL);
	if (value < 0)
		return value;
	if (value != MPU6050_FIFO_ENABLE)
		return -EIO;
	value = i2c_smbus_read_byte_data(client, MPU6050_REG_FIFO_EN);
	if (value < 0)
		return value;
	return value == MPU6050_FIFO_SENSORS ? 0 : -EIO;
}

static void mpu6050_queue_packet(struct mpu6050_state *state, const u8 *data)
{
	struct mpu6050_sample *sample;
	unsigned int index;

	state->sequence++;
	if (!state->opened)
		return;

	/* 느린 앱 때문에 큐가 차면 가장 오래된 샘플을 버리고 유실을 알린다. */
	if (state->queued == MPU6050_QUEUE_SAMPLES) {
		state->head = (state->head + 1) % MPU6050_QUEUE_SAMPLES;
		state->queued--;
		state->queue_overruns++;
		state->error = -EOVERFLOW;
		dev_warn_ratelimited(&state->client->dev, "[QUEUE] oldest sample dropped\n");
	}

	index = (state->head + state->queued) % MPU6050_QUEUE_SAMPLES;
	sample = &state->queue[index];
	sample->sequence = state->sequence;
	sample->accel[0] = (s16)get_unaligned_be16(&data[0]);
	sample->accel[1] = (s16)get_unaligned_be16(&data[2]);
	sample->accel[2] = (s16)get_unaligned_be16(&data[4]);
	/* 온도를 FIFO에 넣지 않으므로 자이로는 바이트 6부터 시작한다. */
	sample->gyro[0] = (s16)get_unaligned_be16(&data[6]);
	sample->gyro[1] = (s16)get_unaligned_be16(&data[8]);
	sample->gyro[2] = (s16)get_unaligned_be16(&data[10]);

	state->queued++;
}

static int mpu6050_drain_fifo(struct mpu6050_state *state)
{
	struct i2c_client *client = state->client;
	u8 count_data[2];
	u8 data[MPU6050_FIFO_CHUNK];
	unsigned int packets;
	int remaining;
	int fifo_count;
	int status;
	int chunk;
	int ret;
	int i;

	ret = i2c_smbus_read_i2c_block_data(client, MPU6050_REG_FIFO_COUNT_H,
					   sizeof(count_data), count_data);
	if (ret < 0)
		return ret;
	if (ret != sizeof(count_data))
		return -EIO;
	fifo_count = get_unaligned_be16(count_data);

	/* count 조회 후 처리 지연을 고려해 세 샘플의 여유를 남긴다. */
	if (fifo_count >= MPU6050_FIFO_BYTES - 3 * MPU6050_PACKET_BYTES) {
		dev_warn_ratelimited(&client->dev, "[FIFO] near full: %d bytes\n", fifo_count);
		return -EOVERFLOW;
	}

	packets = fifo_count / MPU6050_PACKET_BYTES;
	remaining = packets * MPU6050_PACKET_BYTES;
	while (remaining) {
		chunk = min(remaining, MPU6050_FIFO_CHUNK);
		/* FIFO 포트는 매번 같은 주소에서 읽어야 다음 바이트가 나온다. */
		ret = i2c_smbus_read_i2c_block_data(client, MPU6050_REG_FIFO_R_W,
						   chunk, data);
		if (ret < 0)
			return ret;
		if (ret != chunk)
			return -EIO;
		for (i = 0; i < chunk; i += MPU6050_PACKET_BYTES)
			mpu6050_queue_packet(state, &data[i]);
		remaining -= chunk;
	}

	/* 전송 도중 넘쳤다면 이번에 큐에 넣은 데이터도 정상으로 내보내지 않는다. */
	status = i2c_smbus_read_byte_data(client, MPU6050_REG_INT_STATUS);
	if (status < 0)
		return status;
	if (status & MPU6050_FIFO_OVERFLOW) {
		state->fifo_overflows++;
		return -EOVERFLOW;
	}
	if (packets > state->max_batch)
		state->max_batch = packets;
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
	int ret = 0;

	mutex_lock(&state->lock);
	if (state->opened) {
		ret = -EBUSY;
	} else if (state->fatal_error) {
		ret = state->fatal_error;
	} else {
		/* misc 코어가 open과 deregister를 직렬화하는 동안 참조를 확보한다. */
		kref_get(&state->ref);
		state->opened = true;
		state->head = 0;
		state->queued = 0;
		state->error = 0;
		filp->private_data = state;
	}
	mutex_unlock(&state->lock);

	if (ret)
		return ret;
	return nonseekable_open(inode, filp);
}

static int mpu6050_release(struct inode *inode, struct file *filp)
{
	struct mpu6050_state *state = filp->private_data;

	mutex_lock(&state->lock);
	state->opened = false;
	mutex_unlock(&state->lock);

	kref_put(&state->ref, mpu6050_free);
	return 0;
}

static ssize_t mpu6050_read(struct file *filp, char __user *buf,
			    size_t count, loff_t *f_pos)
{
	struct mpu6050_state *state = filp->private_data;
	struct mpu6050_sample *sample;
	int ret;

	if (!count)
		return 0;
	if (count < sizeof(*sample))
		return -EINVAL;

	for (;;) {
		ret = mutex_lock_interruptible(&state->lock);
		if (ret)
			return ret;

		if (!state->client) {
			ret = -ENODEV;
			goto out;
		}
		if (state->fatal_error) {
			ret = state->fatal_error;
			goto out;
		}
		if (state->error) {
			ret = state->error;
			state->error = 0;
			goto out;
		}
		if (state->queued)
			break;

		mutex_unlock(&state->lock);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		/* 상태 mutex를 놓고 자야 IRQ 스레드가 새 샘플을 저장할 수 있다. */
		ret = wait_event_interruptible(state->wait,
			!READ_ONCE(state->client) ||
			READ_ONCE(state->fatal_error) || READ_ONCE(state->error) ||
			READ_ONCE(state->queued));
		if (ret)
			return ret;
	}

	/* 16바이트 복사까지 같은 잠금으로 보호하고 실패하면 큐를 유지한다. */
	sample = &state->queue[state->head];
	if (copy_to_user(buf, sample, sizeof(*sample))) {
		ret = -EFAULT;
		goto out;
	}

	state->head = (state->head + 1) % MPU6050_QUEUE_SAMPLES;
	state->queued--;
	ret = sizeof(*sample);

out:
	mutex_unlock(&state->lock);
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

	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_FIFO_EN, 0x00);
	if (ret < 0)
		dev_warn(&client->dev, "[FIFO] failed to disable channels: %d\n", ret);
	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_USER_CTRL, 0x00);
	if (ret < 0)
		dev_warn(&client->dev, "[FIFO] failed to stop engine: %d\n", ret);

	ret = i2c_smbus_write_byte_data(client, MPU6050_REG_PWR_MGMT_1,
				       MPU6050_SLEEP);
	if (ret < 0)
		dev_warn(&client->dev, "[POWER] failed to enter sleep: %d\n", ret);
}

static irqreturn_t mpu6050_irq_thread(int irq, void *data)
{
	struct mpu6050_state *state = data;
	bool notify = false;
	u32 before;
	int status;
	int reset;
	int ret;

	mutex_lock(&state->lock);

	if (!state->client || state->fatal_error)
		goto out;

	status = i2c_smbus_read_byte_data(state->client, MPU6050_REG_INT_STATUS);
	if (status < 0) {
		ret = status;
		goto recover;
	}
	if (status & MPU6050_FIFO_OVERFLOW) {
		state->fifo_overflows++;
		ret = -EOVERFLOW;
		goto recover;
	}
	if (!(status & MPU6050_DATA_READY))
		goto out;

	state->irq_count++;
	before = state->sequence;
	ret = mpu6050_drain_fifo(state);
	if (ret)
		goto recover;
	notify = state->sequence != before;
	goto out;

recover:
	dev_warn_ratelimited(&state->client->dev,
			     "[FIFO] stream discontinuity: %d, resetting\n", ret);
	state->queued = 0;
	state->fifo_resets++;
	reset = mpu6050_reset_fifo(state->client);
	if (reset) {
		dev_err_ratelimited(&state->client->dev, "[FIFO] reset failed: %d\n", reset);
		state->fatal_error = reset;
		/* 복구 실패 뒤 IRQ 반복을 막고 remove에서 자원을 정리한다. */
		disable_irq_nosync(irq);
	} else {
		state->error = ret;
	}
	notify = true;

out:
	mutex_unlock(&state->lock);
	if (notify)
		wake_up_interruptible(&state->wait);
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

	ret = mpu6050_reset_fifo(client);
	if (ret) {
		dev_err_probe(&client->dev, ret, "[FIFO] setup failed\n");
		goto err_sleep;
	}

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
	ret = mpu6050_write_verify(client, MPU6050_REG_INT_ENABLE, MPU6050_IRQ_ENABLE);
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
	dev_info(&client->dev, "[FIFO] packet=12 bytes, sensor=1024 bytes, queue=128 samples\n");
	dev_info(&client->dev, "[CHAR] /dev/mpu6050 ready: single-reader FIFO read\n");

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

	/* 새 open을 막고 센서 접근을 차단한다. 잠든 read는 깨워서 종료한다. */
	misc_deregister(&state->misc);
	mpu6050_stop_irq(state);
	mpu6050_sleep(client);

	dev_info(&client->dev, "[REMOVE] detached, data-ready count=%lu, samples=%u\n",
		 state->irq_count, state->sequence);
	dev_info(&client->dev,
		 "[FIFO] max-batch=%u, overflows=%lu, resets=%lu, queue-overruns=%lu\n",
		 state->max_batch, state->fifo_overflows, state->fifo_resets, state->queue_overruns);
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
MODULE_DESCRIPTION("MPU6050 hardware FIFO and queued character read practice");
