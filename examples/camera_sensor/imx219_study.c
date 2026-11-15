// SPDX-License-Identifier: GPL-2.0
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <media/v4l2-cci.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>

#define IMX219_WIDTH		3280
#define IMX219_HEIGHT		2464
#define IMX219_FRAME_LENGTH	3526
#define IMX219_LINE_LENGTH	3448
#define IMX219_PIXEL_RATE	182400000
#define IMX219_LINK_FREQ		456000000
#define IMX219_XCLK_FREQ		24000000
#define IMX219_MODE_SELECT	CCI_REG8(0x0100)

struct imx219 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *exposure;
	struct regmap *regmap;
	struct clk *xclk;
	struct regulator_bulk_data supplies[3];
	bool powered;
	bool streaming;
};

/*
 * 6.18 imx219.c의 imx219_common_regs·imx219_2lane_regs 전체와
 * imx219_set_framefmt()의 3280×2464 RAW10 계산값을 사용한다.
 * 문서화되지 않은 초기화 값도 대상 커널과 같은 순서로 유지한다.
 */
static const struct cci_reg_sequence imx219_regs[] = {
	{ IMX219_MODE_SELECT, 0 },
	{ CCI_REG8(0x30eb), 0x05 },
	{ CCI_REG8(0x30eb), 0x0c },
	{ CCI_REG8(0x300a), 0xff },
	{ CCI_REG8(0x300b), 0xff },
	{ CCI_REG8(0x30eb), 0x05 },
	{ CCI_REG8(0x30eb), 0x09 },
	{ CCI_REG8(0x455e), 0x00 },
	{ CCI_REG8(0x471e), 0x4b },
	{ CCI_REG8(0x4767), 0x0f },
	{ CCI_REG8(0x4750), 0x14 },
	{ CCI_REG8(0x4540), 0x00 },
	{ CCI_REG8(0x47b4), 0x14 },
	{ CCI_REG8(0x4713), 0x30 },
	{ CCI_REG8(0x478b), 0x10 },
	{ CCI_REG8(0x478f), 0x10 },
	{ CCI_REG8(0x4793), 0x10 },
	{ CCI_REG8(0x4797), 0x0e },
	{ CCI_REG8(0x479b), 0x0e },
	{ CCI_REG8(0x0170), 1 },
	{ CCI_REG8(0x0171), 1 },
	{ CCI_REG8(0x0128), 0 },
	{ CCI_REG16(0x012a), 24 * 256 },
	{ CCI_REG8(0x0301), 5 },
	{ CCI_REG8(0x0303), 1 },
	{ CCI_REG8(0x0304), 3 },
	{ CCI_REG8(0x0305), 3 },
	{ CCI_REG16(0x0306), 57 },
	{ CCI_REG8(0x030b), 1 },
	{ CCI_REG16(0x030c), 114 },
	{ CCI_REG8(0x0114), 1 },
	{ CCI_REG16(0x0164), 0 },
	{ CCI_REG16(0x0166), IMX219_WIDTH - 1 },
	{ CCI_REG16(0x0168), 0 },
	{ CCI_REG16(0x016a), IMX219_HEIGHT - 1 },
	{ CCI_REG8(0x0174), 0 },
	{ CCI_REG8(0x0175), 0 },
	{ CCI_REG16(0x016c), IMX219_WIDTH },
	{ CCI_REG16(0x016e), IMX219_HEIGHT },
	{ CCI_REG16(0x0624), IMX219_WIDTH },
	{ CCI_REG16(0x0626), IMX219_HEIGHT },
	{ CCI_REG16(0x018c), 0x0a0a },
	{ CCI_REG8(0x0309), 10 },
	{ CCI_REG16(0x0162), IMX219_LINE_LENGTH },
};

static const s64 imx219_link_freq[] = { IMX219_LINK_FREQ };
static const char * const imx219_test_patterns[] = {
	"Disabled", "Color Bars",
};

static int imx219_check_endpoint(struct device *dev)
{
	struct v4l2_fwnode_endpoint ep = {
		.bus_type = V4L2_MBUS_CSI2_DPHY,
	};
	struct fwnode_handle *endpoint;
	unsigned int i;
	int ret;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint)
		return dev_err_probe(dev, -EINVAL, "missing endpoint\n");

	ret = v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep);
	fwnode_handle_put(endpoint);
	if (ret)
		goto out;

	ret = -EINVAL;
	if (ep.bus.mipi_csi2.num_data_lanes != 2)
		goto out;

	for (i = 0; i < ep.nr_of_link_frequencies; i++) {
		if (ep.link_frequencies[i] == IMX219_LINK_FREQ) {
			ret = 0;
			break;
		}
	}

out:
	v4l2_fwnode_endpoint_free(&ep);
	if (ret)
		return dev_err_probe(dev, ret, "invalid CSI-2 endpoint\n");

	return 0;
}

static int imx219_power_on(struct imx219 *sensor)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(sensor->supplies),
				    sensor->supplies);
	if (ret)
		return ret;

	ret = clk_prepare_enable(sensor->xclk);
	if (ret) {
		regulator_bulk_disable(ARRAY_SIZE(sensor->supplies),
				       sensor->supplies);
		return ret;
	}

	usleep_range(6200, 7200);

	return 0;
}

static void imx219_power_off(struct imx219 *sensor)
{
	clk_disable_unprepare(sensor->xclk);
	regulator_bulk_disable(ARRAY_SIZE(sensor->supplies), sensor->supplies);
}

static int imx219_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx219 *sensor = container_of(ctrl->handler, struct imx219,
					   ctrl_handler);
	int ret;

	if (ctrl->id == V4L2_CID_VBLANK) {
		ret = __v4l2_ctrl_modify_range(sensor->exposure, 4,
					       IMX219_HEIGHT + ctrl->val - 4,
					       1, 1600);
		if (ret)
			return ret;
	}

	/* setup()도 같은 handler lock 안에서 호출하므로 전원 상태를 검사한다. */
	if (!sensor->powered)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		return cci_write(sensor->regmap, CCI_REG16(0x015a),
				 ctrl->val, NULL);
	case V4L2_CID_ANALOGUE_GAIN:
		return cci_write(sensor->regmap, CCI_REG8(0x0157),
				 ctrl->val, NULL);
	case V4L2_CID_VBLANK:
		return cci_write(sensor->regmap, CCI_REG16(0x0160),
				 IMX219_HEIGHT + ctrl->val, NULL);
	case V4L2_CID_TEST_PATTERN:
		return cci_write(sensor->regmap, CCI_REG16(0x0600),
				 ctrl->val ? 2 : 0, NULL);
	default:
		return -EINVAL;
	}
}

static const struct v4l2_ctrl_ops imx219_ctrl_ops = {
	.s_ctrl = imx219_set_ctrl,
};

static int imx219_init_controls(struct imx219 *sensor)
{
	struct v4l2_ctrl_handler *hdl = &sensor->ctrl_handler;
	struct v4l2_ctrl *ctrl;
	int ret;

	ret = v4l2_ctrl_handler_init(hdl, 7);
	if (ret)
		return ret;

	sensor->exposure = v4l2_ctrl_new_std(hdl, &imx219_ctrl_ops,
					     V4L2_CID_EXPOSURE, 4,
					     IMX219_FRAME_LENGTH - 4, 1, 1600);
	v4l2_ctrl_new_std(hdl, &imx219_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  0, 232, 1, 0);
	v4l2_ctrl_new_std(hdl, &imx219_ctrl_ops, V4L2_CID_VBLANK,
			  32, 0xfffe - IMX219_HEIGHT, 1,
			  IMX219_FRAME_LENGTH - IMX219_HEIGHT);

	ctrl = v4l2_ctrl_new_std(hdl, NULL, V4L2_CID_HBLANK,
				IMX219_LINE_LENGTH - IMX219_WIDTH,
				IMX219_LINE_LENGTH - IMX219_WIDTH, 1,
				IMX219_LINE_LENGTH - IMX219_WIDTH);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std(hdl, NULL, V4L2_CID_PIXEL_RATE,
			  IMX219_PIXEL_RATE, IMX219_PIXEL_RATE, 1,
			  IMX219_PIXEL_RATE);
	ctrl = v4l2_ctrl_new_int_menu(hdl, NULL, V4L2_CID_LINK_FREQ,
				     0, 0, imx219_link_freq);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std_menu_items(hdl, &imx219_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(imx219_test_patterns) - 1,
				     0, 0, imx219_test_patterns);
	if (hdl->error)
		return hdl->error;

	sensor->sd.ctrl_handler = hdl;

	return 0;
}

static int imx219_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx219 *sensor = container_of(sd, struct imx219, sd);
	int ret = 0;

	mutex_lock(sensor->ctrl_handler.lock);
	if (sensor->streaming == !!enable)
		goto unlock;

	if (!enable) {
		ret = cci_write(sensor->regmap, IMX219_MODE_SELECT, 0, NULL);
		goto power_off;
	}

	ret = imx219_power_on(sensor);
	if (ret)
		goto unlock;

	sensor->powered = true;
	ret = cci_multi_reg_write(sensor->regmap, imx219_regs,
				  ARRAY_SIZE(imx219_regs), NULL);
	if (ret)
		goto power_off;

	/* handler lock을 유지한 채 저장된 컨트롤 값을 센서에 적용한다. */
	ret = __v4l2_ctrl_handler_setup(&sensor->ctrl_handler);
	if (ret)
		goto power_off;

	ret = cci_write(sensor->regmap, IMX219_MODE_SELECT, 1, NULL);
	if (ret)
		goto power_off;

	sensor->streaming = true;
	goto unlock;

power_off:
	imx219_power_off(sensor);
	sensor->powered = false;
	sensor->streaming = false;
unlock:
	mutex_unlock(sensor->ctrl_handler.lock);

	return ret;
}

static int imx219_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index)
		return -EINVAL;

	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;

	return 0;
}

static int imx219_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->pad || fse->index || fse->code != MEDIA_BUS_FMT_SRGGB10_1X10)
		return -EINVAL;

	fse->min_width = IMX219_WIDTH;
	fse->max_width = IMX219_WIDTH;
	fse->min_height = IMX219_HEIGHT;
	fse->max_height = IMX219_HEIGHT;

	return 0;
}

static int imx219_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *state,
			  struct v4l2_subdev_format *fmt)
{
	if (fmt->pad)
		return -EINVAL;

	fmt->format = (struct v4l2_mbus_framefmt) {
		.width = IMX219_WIDTH,
		.height = IMX219_HEIGHT,
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.field = V4L2_FIELD_NONE,
		.colorspace = V4L2_COLORSPACE_RAW,
		.ycbcr_enc = V4L2_YCBCR_ENC_601,
		.quantization = V4L2_QUANTIZATION_FULL_RANGE,
		.xfer_func = V4L2_XFER_FUNC_NONE,
	};

	return 0;
}

static const struct v4l2_subdev_pad_ops imx219_pad_ops = {
	.enum_mbus_code = imx219_enum_mbus_code,
	.enum_frame_size = imx219_enum_frame_size,
	.get_fmt = imx219_get_fmt,
	.set_fmt = imx219_get_fmt,
};

static const struct v4l2_subdev_video_ops imx219_video_ops = {
	.s_stream = imx219_s_stream,
};

static const struct v4l2_subdev_ops imx219_subdev_ops = {
	.video = &imx219_video_ops,
	.pad = &imx219_pad_ops,
};

static int imx219_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct imx219 *sensor;
	u64 chip_id;
	int ret;

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&sensor->sd, client, &imx219_subdev_ops);
	ret = imx219_check_endpoint(dev);
	if (ret)
		return ret;

	sensor->regmap = devm_cci_regmap_init_i2c(client, 16);
	if (IS_ERR(sensor->regmap))
		return dev_err_probe(dev, PTR_ERR(sensor->regmap),
				     "failed to initialize CCI\n");

	sensor->xclk = devm_v4l2_sensor_clk_get(dev, NULL);
	if (IS_ERR(sensor->xclk))
		return dev_err_probe(dev, PTR_ERR(sensor->xclk),
				     "failed to get xclk\n");
	if (clk_get_rate(sensor->xclk) != IMX219_XCLK_FREQ)
		return dev_err_probe(dev, -EINVAL, "xclk must be 24 MHz\n");

	sensor->supplies[0].supply = "VANA";
	sensor->supplies[1].supply = "VDIG";
	sensor->supplies[2].supply = "VDDL";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(sensor->supplies),
				      sensor->supplies);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get supplies\n");

	ret = imx219_power_on(sensor);
	if (ret)
		return dev_err_probe(dev, ret, "failed to power on\n");

	ret = cci_read(sensor->regmap, CCI_REG16(0x0000), &chip_id, NULL);
	imx219_power_off(sensor);
	if (ret)
		return dev_err_probe(dev, ret, "failed to read chip ID\n");
	if (chip_id != 0x0219)
		return dev_err_probe(dev, -ENODEV,
				     "unexpected chip ID: 0x%04llx\n", chip_id);

	dev_info(dev, "chip ID: 0x%04llx\n", chip_id);
	/* LP-11용 토글은 첫 스트림의 보드 검증 결과에 따라 추가한다. */
	ret = imx219_init_controls(sensor);
	if (ret)
		goto free_controls;

	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
	if (ret)
		goto free_controls;

	ret = v4l2_async_register_subdev_sensor(&sensor->sd);
	if (ret)
		goto cleanup_entity;

	return 0;

cleanup_entity:
	media_entity_cleanup(&sensor->sd.entity);
free_controls:
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);

	return ret;
}

static void imx219_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx219 *sensor = container_of(sd, struct imx219, sd);

	v4l2_async_unregister_subdev(sd);
	imx219_s_stream(sd, 0);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&sensor->ctrl_handler);
}

static const struct of_device_id imx219_dt_ids[] = {
	{ .compatible = "nakta,imx219-study" },
	{ }
};
MODULE_DEVICE_TABLE(of, imx219_dt_ids);

static struct i2c_driver imx219_driver = {
	.driver = {
		.name = "imx219-study",
		.of_match_table = imx219_dt_ids,
	},
	.probe = imx219_probe,
	.remove = imx219_remove,
};
module_i2c_driver(imx219_driver);

MODULE_DESCRIPTION("Fixed-mode IMX219 sensor study driver");
MODULE_LICENSE("GPL");
