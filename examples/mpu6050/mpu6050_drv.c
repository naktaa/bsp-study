#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

#define MPU6050_REG_WHO_AM_I	0x75
#define MPU6050_WHO_AM_I		0x68

static int mpu6050_probe(struct i2c_client *client)
{
	int ret;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_READ_BYTE_DATA))
		return dev_err_probe(&client->dev, -EOPNOTSUPP,
				     "[PROBE] byte data transfer unsupported\n");

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
	return 0;
}

static void mpu6050_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "[REMOVE] detached\n");
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
MODULE_DESCRIPTION("MPU6050 I2C identification practice");
