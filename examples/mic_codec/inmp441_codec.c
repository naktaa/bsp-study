// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <sound/soc.h>

/* INMP441 데이터시트 WS 7.8~50kHz: docs/hardware.md INMP441 항목 */
#define INMP441_RATE_MIN 7800
#define INMP441_RATE_MAX 50000

static struct snd_soc_dai_driver inmp441_dai = {
	.name = "inmp441-hifi",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = INMP441_RATE_MIN,
		.rate_max = INMP441_RATE_MAX,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.formats = SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	},
};

static const struct snd_soc_component_driver inmp441_component_driver = {
};

static int inmp441_probe(struct platform_device *pdev)
{
	int ret;

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &inmp441_component_driver,
					      &inmp441_dai, 1);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "Failed to register codec component\n");

	dev_info(&pdev->dev,
		 "[CODEC] registered: 7800-50000 Hz, S24_LE/S32_LE\n");

	return 0;
}

static const struct of_device_id inmp441_ids[] = {
	{ .compatible = "nakta,inmp441" },
	{ }
};
MODULE_DEVICE_TABLE(of, inmp441_ids);

static struct platform_driver inmp441_driver = {
	.driver = {
		.name = "inmp441-codec",
		.of_match_table = inmp441_ids,
	},
	.probe = inmp441_probe,
};
module_platform_driver(inmp441_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASoC INMP441 microphone codec driver");
MODULE_AUTHOR("Nakta");
