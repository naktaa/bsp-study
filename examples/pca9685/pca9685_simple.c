// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/math64.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/pwm.h>

#define PCA9685_MODE1		0x00
#define PCA9685_LED_ON_L(n)	(0x06 + 4 * (n))
#define PCA9685_ALL_LED_ON_L	0xfa
#define PCA9685_PRESCALE		0xfe

#define MODE1_SLEEP		BIT(4)
#define MODE1_AI			BIT(5)
#define LED_FULL		BIT(4)

#define PCA9685_PERIOD_NS		(4096 * 40)

struct pca9685 {
	struct i2c_client *client;
	struct mutex lock;
	unsigned long requested;
};

static int pca9685_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct pca9685 *pca = pwmchip_get_drvdata(chip);

	guard(mutex)(&pca->lock);
	pca->requested |= BIT(pwm->hwpwm);

	return 0;
}

static void pca9685_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct pca9685 *pca = pwmchip_get_drvdata(chip);

	guard(mutex)(&pca->lock);
	pca->requested &= ~BIT(pwm->hwpwm);
}

static int pca9685_set_sleep(struct pca9685 *pca, bool sleep)
{
	int mode;

	mode = i2c_smbus_read_byte_data(pca->client, PCA9685_MODE1);
	if (mode < 0)
		return mode;

	mode = (mode & ~MODE1_SLEEP) | (sleep ? MODE1_SLEEP : 0);
	return i2c_smbus_write_byte_data(pca->client, PCA9685_MODE1, mode);
}

static void pca9685_cleanup_sleep(void *data)
{
	struct pca9685 *pca = data;
	int ret;

	ret = pca9685_set_sleep(pca, true);
	if (ret < 0)
		dev_err(&pca->client->dev, "Failed to enter sleep: %d\n", ret);
}

static int pca9685_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			 const struct pwm_state *state)
{
	struct pca9685 *pca = pwmchip_get_drvdata(chip);
	u8 onoff[4] = { 0, 0, 0, LED_FULL };
	unsigned int prescale, quantum_ns, duty;
	int current_prescale, ret;

	guard(mutex)(&pca->lock);

	if (state->polarity != PWM_POLARITY_NORMAL)
		return -EINVAL;

	if (!state->enabled)
		goto write;

	if (state->period < PCA9685_PERIOD_NS * 4)
		return -EINVAL;

	prescale = min_t(u64, div_u64(state->period, PCA9685_PERIOD_NS) - 1, 255);

	current_prescale = i2c_smbus_read_byte_data(pca->client, PCA9685_PRESCALE);
	if (current_prescale < 0)
		return current_prescale;

	if (current_prescale != prescale) {
		/* 다른 request 채널이 있으면 공유 prescale을 바꿀 수 없다. */
		if (pca->requested & ~BIT(pwm->hwpwm))
			return -EINVAL;

		/* PRE_SCALE은 SLEEP 중에만 쓸 수 있다. */
		ret = pca9685_set_sleep(pca, true);
		if (ret < 0)
			return ret;

		ret = i2c_smbus_write_byte_data(pca->client, PCA9685_PRESCALE,
						prescale);
		if (ret < 0)
			return ret;

		ret = pca9685_set_sleep(pca, false);
		if (ret < 0)
			return ret;

		usleep_range(500, 1000);
	}

	quantum_ns = 40 * (prescale + 1);
	duty = div_u64(min_t(u64, state->duty_cycle, 4096 * quantum_ns), quantum_ns);

	if (duty >= 4096) {
		onoff[1] = LED_FULL;
		onoff[3] = 0;
	} else if (duty) {
		onoff[2] = duty & 0xff;
		onoff[3] = (duty >> 8) & 0xf;
	}

write:
	return i2c_smbus_write_i2c_block_data(pca->client,
					    PCA9685_LED_ON_L(pwm->hwpwm),
					    sizeof(onoff), onoff);
}

static int pca9685_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
			     struct pwm_state *state)
{
	struct pca9685 *pca = pwmchip_get_drvdata(chip);
	unsigned int quantum_ns, on, off;
	u8 onoff[4] = {};
	int prescale, ret;

	guard(mutex)(&pca->lock);

	ret = i2c_smbus_read_i2c_block_data(pca->client,
					 PCA9685_LED_ON_L(pwm->hwpwm),
					 sizeof(onoff), onoff);
	if (ret < 0)
		return ret;

	prescale = i2c_smbus_read_byte_data(pca->client, PCA9685_PRESCALE);
	if (prescale < 0)
		return prescale;

	quantum_ns = 40 * (prescale + 1);
	on = onoff[0] | (onoff[1] & 0xf) << 8;
	off = onoff[2] | (onoff[3] & 0xf) << 8;
	state->period = 4096 * quantum_ns;
	state->polarity = PWM_POLARITY_NORMAL;
	state->enabled = !(onoff[3] & LED_FULL);
	if (!state->enabled)
		state->duty_cycle = 0;
	else if (onoff[1] & LED_FULL)
		state->duty_cycle = state->period;
	else
		state->duty_cycle = ((off - on) & 0xfff) * quantum_ns;

	return 0;
}

static const struct pwm_ops pca9685_ops = {
	.request = pca9685_request,
	.free = pca9685_free,
	.apply = pca9685_apply,
	.get_state = pca9685_get_state,
};

static int pca9685_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	const u8 full_off[4] = { 0, 0, 0, LED_FULL };
	struct pwm_chip *chip;
	struct pca9685 *pca;
	int ret;

	chip = devm_pwmchip_alloc(dev, 16, sizeof(*pca));
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	pca = pwmchip_get_drvdata(chip);
	pca->client = client;
	mutex_init(&pca->lock);
	chip->ops = &pca9685_ops;

	/* 업스트림 해제 뒤에도 LEDn 값이 남으므로 깨우기 전에 모두 끈다. */
	ret = i2c_smbus_write_byte_data(client, PCA9685_MODE1,
					MODE1_AI | MODE1_SLEEP);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to initialize MODE1\n");

	ret = i2c_smbus_write_i2c_block_data(client, PCA9685_ALL_LED_ON_L,
					     sizeof(full_off), full_off);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_write_byte_data(client, PCA9685_MODE1, MODE1_AI);
	if (ret < 0)
		return ret;

	usleep_range(500, 1000);

	ret = devm_add_action_or_reset(dev, pca9685_cleanup_sleep, pca);
	if (ret < 0)
		return ret;

	return devm_pwmchip_add(dev, chip);
}

static const struct of_device_id pca9685_of_match[] = {
	{ .compatible = "nxp,pca9685-pwm" },
	{ }
};
MODULE_DEVICE_TABLE(of, pca9685_of_match);

static const struct i2c_device_id pca9685_id[] = {
	{ "pca9685" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pca9685_id);

static struct i2c_driver pca9685_driver = {
	.driver = {
		.name = "pca9685-simple",
		.of_match_table = pca9685_of_match,
	},
	.probe = pca9685_probe,
	.id_table = pca9685_id,
};
module_i2c_driver(pca9685_driver);

MODULE_DESCRIPTION("Simple PCA9685 PWM provider");
MODULE_LICENSE("GPL");
