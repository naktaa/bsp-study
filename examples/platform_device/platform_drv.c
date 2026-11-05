#include <linux/device.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

struct platform_state {
	const char *label;
	u32 study_value;
};

static ssize_t label_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	const struct platform_state *state = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%s\n", state->label);
}
static DEVICE_ATTR_RO(label);

static ssize_t study_value_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	const struct platform_state *state = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", state->study_value);
}
static DEVICE_ATTR_RO(study_value);

static struct attribute *platform_attrs[] = {
	&dev_attr_label.attr,
	&dev_attr_study_value.attr,
	NULL,
};

static const struct attribute_group platform_group = {
	.attrs = platform_attrs,
};

static int platform_probe(struct platform_device *pdev)
{
	struct platform_state *state;
	const char *label;
	int ret;

	state = devm_kzalloc(&pdev->dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	ret = of_property_read_string(pdev->dev.of_node, "label", &label);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "[PROBE] failed to read label\n");

	ret = of_property_read_u32(pdev->dev.of_node, "nakta,study-value",
				   &state->study_value);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "[PROBE] failed to read nakta,study-value\n");

	/* sysfs 조회에 사용할 문자열을 장치 소유 메모리에 보관한다. */
	state->label = devm_kstrdup(&pdev->dev, label, GFP_KERNEL);
	if (!state->label)
		return -ENOMEM;

	platform_set_drvdata(pdev, state);

	/* 값과 조회 문맥을 준비한 뒤 속성을 공개한다. */
	ret = devm_device_add_group(&pdev->dev, &platform_group);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
				     "[PROBE] failed to create sysfs attributes\n");

	dev_info(&pdev->dev, "[PROBE] label=%s, value=%u\n",
		 state->label, state->study_value);
	return 0;
}

static void platform_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "[REMOVE] detached\n");
}

static const struct of_device_id platform_match[] = {
	{ .compatible = "nakta,platform-demo" },
	{},
};
MODULE_DEVICE_TABLE(of, platform_match);

static struct platform_driver platform_driver = {
	.probe = platform_probe,
	.remove = platform_remove,
	.driver = {
		.name = "platform-demo",
		.of_match_table = platform_match,
	},
};
module_platform_driver(platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nakta");
MODULE_DESCRIPTION("Device Tree platform driver matching practice");
