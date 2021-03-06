/*****************************************************************************
* Copyright 2011 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <mach/vcio.h>
#include <linux/thermal.h>


/* --- DEFINITIONS --- */
#define MODULE_NAME "bcm2835_thermal"

/*#define THERMAL_DEBUG_ENABLE*/

#ifdef THERMAL_DEBUG_ENABLE
#define print_debug(fmt,...) printk(KERN_INFO "%s:%s:%d: "fmt"\n", MODULE_NAME, __func__, __LINE__, ##__VA_ARGS__)
#else
#define print_debug(fmt,...)
#endif
#define print_err(fmt,...) printk(KERN_ERR "%s:%s:%d: "fmt"\n", MODULE_NAME, __func__,__LINE__, ##__VA_ARGS__)

#define VC_TAG_GET_TEMP 0x00030006
#define VC_TAG_GET_MAX_TEMP 0x0003000A

typedef enum {
	TEMP,
	MAX_TEMP,
} temp_type;

/* --- STRUCTS --- */
/* tag part of the message */
struct vc_msg_tag {
	uint32_t tag_id;		/* the tag ID for the temperature */
	uint32_t buffer_size;	/* size of the buffer (should be 8) */
	uint32_t request_code;	/* identifies message as a request (should be 0) */
	uint32_t id;			/* extra ID field (should be 0) */
	uint32_t val;			/* returned value of the temperature */
};

/* message structure to be sent to videocore */
struct vc_msg {
	uint32_t msg_size;		/* simply, sizeof(struct vc_msg) */
	uint32_t request_code;		/* holds various information like the success and number of bytes returned (refer to mailboxes wiki) */
	struct vc_msg_tag tag;		/* the tag structure above to make */
	uint32_t end_tag;		/* an end identifier, should be set to NULL */
};

struct bcm2835_thermal_data {
	struct thermal_zone_device *thermal_dev;
	struct vc_msg msg;
};

/* --- GLOBALS --- */
static struct bcm2835_thermal_data bcm2835_data;

/* Thermal Device Operations */
static struct thermal_zone_device_ops ops;

/* --- FUNCTIONS --- */

static int bcm2835_get_temp_or_max(struct thermal_zone_device *thermal_dev, unsigned long *temp, unsigned tag_id)
{
	int result = -1, retry = 3;
	print_debug("IN");

	*temp = 0;
	while (result != 0 && retry-- > 0) {
		/* wipe all previous message data */
		memset(&bcm2835_data.msg, 0, sizeof bcm2835_data.msg);

		/* prepare message */
		bcm2835_data.msg.msg_size = sizeof bcm2835_data.msg;
		bcm2835_data.msg.tag.buffer_size = 8;
		bcm2835_data.msg.tag.tag_id = tag_id;

		/* send the message */
		result = bcm_mailbox_property(&bcm2835_data.msg, sizeof bcm2835_data.msg);
		print_debug("Got %stemperature as %u (%d,%x)\n", tag_id==VC_TAG_GET_MAX_TEMP ? "max ":"", (uint)bcm2835_data.msg.tag.val, result, bcm2835_data.msg.request_code);
		if (!(bcm2835_data.msg.request_code & 0x80000000))
			result = -1;
	}

	/* check if it was all ok and return the rate in milli degrees C */
	if (result == 0)
		*temp = (uint)bcm2835_data.msg.tag.val;
	else
		print_err("Failed to get temperature! (%x:%d)\n", tag_id, result);
	print_debug("OUT");
	return result;
}

static int bcm2835_get_temp(struct thermal_zone_device *thermal_dev, unsigned long *temp)
{
	return bcm2835_get_temp_or_max(thermal_dev, temp, VC_TAG_GET_TEMP);
}

static int bcm2835_get_max_temp(struct thermal_zone_device *thermal_dev, int trip_num, unsigned long *temp)
{
	return bcm2835_get_temp_or_max(thermal_dev, temp, VC_TAG_GET_MAX_TEMP);
}

static int bcm2835_get_trip_type(struct thermal_zone_device * thermal_dev, int trip_num, enum thermal_trip_type *trip_type)
{
	*trip_type = THERMAL_TRIP_HOT;
	return 0;
}


static int bcm2835_get_mode(struct thermal_zone_device *thermal_dev, enum thermal_device_mode *dev_mode)
{
	*dev_mode = THERMAL_DEVICE_ENABLED;
	return 0;
}


static int bcm2835_thermal_probe(struct platform_device *pdev)
{
	print_debug("IN");
	print_debug("THERMAL Driver has been probed!");

	/* check that the device isn't null!*/
	if(pdev == NULL)
	{
		print_debug("Platform device is empty!");
		return -ENODEV;
	}

	if(!(bcm2835_data.thermal_dev = thermal_zone_device_register("bcm2835_thermal",	1, 0, NULL, &ops,1,1,1000,1000)))
	{
		print_debug("Unable to register the thermal device!");
		return -EFAULT;
	}
	return 0;
}


static int bcm2835_thermal_remove(struct platform_device *pdev)
{
	print_debug("IN");

	thermal_zone_device_unregister(bcm2835_data.thermal_dev);

	print_debug("OUT");

	return 0;
}

static struct thermal_zone_device_ops ops  = {
	.get_temp = bcm2835_get_temp,
	.get_trip_temp = bcm2835_get_max_temp,
	.get_trip_type = bcm2835_get_trip_type,
	.get_mode = bcm2835_get_mode,
};

/* Thermal Driver */
static struct platform_driver bcm2835_thermal_driver = {
	.probe = bcm2835_thermal_probe,
	.remove = bcm2835_thermal_remove,
	.driver = {
				.name = "bcm2835_thermal",
				.owner = THIS_MODULE,
			},
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dorian Peake");
MODULE_DESCRIPTION("Thermal driver for bcm2835 chip");

module_platform_driver(bcm2835_thermal_driver);
