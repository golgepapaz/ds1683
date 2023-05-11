
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/hwmon-sysfs.h>

/*Registers*/
#define DS1683_REG_COMMAND        0x00
#define DS1683_REG_STATUS         0x01
#define DS1683_REG_PWE            0x02
#define DS1683_REG_EVENT_COUNTER  0x08
#define DS1683_REG_ELAPSED        0x0A
#define DS1683_REG_EVENT_COUNTER_ALARM 0x10
#define DS1683_REG_ELAPSED_ALARM  0x12
#define DS1683_REG_CONFIG         0x16
#define DS1683_REG_EEPROM         0x20


static ssize_t ds1683_show(struct device * dev, struct device_attribute *attr,
                char *buf)
{
    struct sensor_device_attribute_2 *sattr = to_sensor_dev_attr_2(attr);
    struct i2c_client *client = to_i2c_client(dev);
    unsigned long long val, check;
    __le32 val_le = 0;
    int rc;

    dev_dbg(dev, "ds1683_show() called on %s\n", attr->attr.name);

    /* Read the register */
    rc = i2c_smbus_read_i2c_block_data(client, sattr->index, sattr->nr,
                       (u8 *)&val_le);
    if (rc < 0)
        return -EIO;

    val = le32_to_cpu(val_le);

    if (sattr->index == DS1683_REG_ELAPSED) {
        int retries = 5;

        /* Detect and retry when a tick occurs mid-read */
        do {
            rc = i2c_smbus_read_i2c_block_data(client, sattr->index,
                               sattr->nr,
                               (u8 *)&val_le);
            if (rc < 0 || retries <= 0)
                return -EIO;

            check = val;
            val = le32_to_cpu(val_le);
            retries--;
        } while (val != check && val != (check + 1));
    }
	return sprintf(buf, "%llu\n", (sattr->nr == 4) ? (val * 250) : val);
}

static ssize_t ds1683_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct sensor_device_attribute_2 *sattr = to_sensor_dev_attr_2(attr);
	struct i2c_client *client = to_i2c_client(dev);
	u64 val;
	__le32 val_le;
	int rc;

	dev_dbg(dev, "ds1682_store() called on %s\n", attr->attr.name);

	/* Decode input */
	rc = kstrtoull(buf, 0, &val);
	if (rc < 0) {
		dev_dbg(dev, "input string not a number\n");
		return -EINVAL;
	}

	/* Special case: the 32 bit regs are time values with 1/4s
	 * resolution, scale input down to quarter-seconds */
	if (sattr->nr == 4)
		do_div(val, 250);

	/* write out the value */
	val_le = cpu_to_le32(val);
	rc = i2c_smbus_write_i2c_block_data(client, sattr->index, sattr->nr,
					    (u8 *) & val_le);
	if (rc < 0) {
		dev_err(dev, "register write failed; reg=0x%x, size=%i\n",
			sattr->index, sattr->nr);
		return -EIO;
	}

	return count;
}
/*
 * Simple register attributes
 */
static SENSOR_DEVICE_ATTR_2(elapsed_time, S_IRUGO | S_IWUSR, ds1683_show,
			    ds1683_store, 4, DS1683_REG_ELAPSED);
static SENSOR_DEVICE_ATTR_2(elapsed_time_alarm, S_IRUGO | S_IWUSR, ds1683_show,
			    ds1683_store, 2, DS1683_REG_ELAPSED_ALARM);
static SENSOR_DEVICE_ATTR_2(event_count, S_IRUGO | S_IWUSR, ds1683_show,
			    ds1683_store, 2, DS1683_REG_EVENT_COUNTER);
static SENSOR_DEVICE_ATTR_2(event_count_alarm, S_IRUGO | S_IWUSR, ds1683_show,
			    ds1683_store, 2, DS1683_REG_EVENT_COUNTER_ALARM);
static const struct attribute_group ds1683_group = {
	.attrs = (struct attribute *[]) {
		&sensor_dev_attr_elapsed_time.dev_attr.attr,
		&sensor_dev_attr_elapsed_time_alarm.dev_attr.attr,
		&sensor_dev_attr_event_count.dev_attr.attr,
        &sensor_dev_attr_event_count_alarm.dev_attr.attr,
		NULL,
	},
};

static int ds1683_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    int rc;

    if (!i2c_check_functionality(client->adapter,
                     I2C_FUNC_SMBUS_I2C_BLOCK)) {
        dev_err(&client->dev, "i2c bus does not support the ds1683\n");
        rc = -ENODEV;
        return rc;
    }

    rc = sysfs_create_group(&client->dev.kobj, &ds1683_group);
    return rc;

}
static int ds1683_remove(struct i2c_client *client)
{
    sysfs_remove_group(&client->dev.kobj, &ds1683_group);
    return 0;
}

static const struct i2c_device_id ds1683_id[] = {
    { "ds1683", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, ds1683_id);

static const struct of_device_id ds1683_of_match[] = {
    { .compatible = "dallas,ds1683" },
    { }
};
MODULE_DEVICE_TABLE(of, ds1683_of_match);



static struct i2c_driver ds1683_driver = {
    .driver = {
        .name = "ds1683",
        .of_match_table = ds1683_of_match,
    },
    .probe = ds1683_probe,
    .remove = ds1683_remove,
    .id_table = ds1683_id,  
};

module_i2c_driver(ds1683_driver);

MODULE_AUTHOR("Hurcan Solter <hsolter@gmail.com>");
MODULE_DESCRIPTION("Dallas DS1683 RTC driver");
MODULE_LICENSE("GPL");

