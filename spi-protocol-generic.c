/*
 * Simple protocol driver for generic SPI device.
 * Targeted to communcate with Arduino, but nowdays
 * supports only loopback mode
 *
 * Copyright (C) 2017 Georgiy Odisharia
 *	<math.kraut.cat@gmail.com>
 *
 * This program is distributed under GPLv2 license.
 * See LICENSE.md for more information.
 */

#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/spi/spi.h>

#include "spi-protocol-generic.h"

#ifdef DEBUG_SPI_PROTOCOL_GENERIC
#define debug_printk(...) printk(KERN_DEBUG "spi-protocol-generic: "); \
		printk(KERN_CONT __VA_ARGS__); \
		printk(KERN_CONT "\n")
#else /* DEBUG_SPI_PROTOCOL_GENERIC */
#define debug_printk(...) do {} while(0)
#endif /* DEBUG_SPI_PROTOCOL_GENERIC */

// Protocol info

#define MAGIX_TRANSACTION 0xFA
#define GET_REGISTER	  0xDE
#define SET_REGISTER      0xDB

static u8 request_registry = MAGIX_TRANSACTION;
static u8 get_register	   = GET_REGISTER;
static u8 set_register     = SET_REGISTER;

// Internal info

#define SPI_CLASS_NAME			"spi_protocol_generic"
#define SPI_PROTOCOL_GENERIC_DEVICE	"pro_mini_spi_generic"

static dev_t first_dev_t;
static unsigned minorcount;
static struct class		*spi_protocol_generic_class;

struct meta_information {
	struct spi_device *device;

	dev_t device_maj_min;
	struct cdev *internal_cdev;

	u8 *tx_buffer_reg;
	u8 *rx_buffer_reg;

	struct list_head list_entry;
};

static LIST_HEAD(meta_info_list);
static DEFINE_MUTEX(device_list_lock);

static const unsigned buf_reg_size = 4;
static const unsigned n_reg_xfers = 4;

#ifdef MY_BUGGY_DT_OVERLAY
enum {
	pro_mini,
	due,
};

struct arduino_board_info {
	u32 spi_frequency;
};

static const struct arduino_board_info arduino_info[] = {
	[pro_mini] = {
		.spi_frequency = 500000,
	},
	[due] = {
		.spi_frequency = 4000000,
	},
};
#endif /* MY_BUGGY_DT_OVERLAY */

#ifdef CONFIG_OF
static const struct of_device_id spi_protocol_generic_of_match[] = {
	{
		.compatible = "arduino,pro-mini-spi-generic",
#ifdef MY_BUGGY_DT_OVERLAY
		.data = &arduino_info[pro_mini],
#endif /* MY_BUGGY_DT_OVERLAY */
	},
	{
		.compatible = "arduino,due-spi-generic",
#ifdef MY_BUGGY_DT_OVERLAY
		.data = &arduino_info[due],
#endif /* MY_BUGGY_DT_OVERLAY */
	},
	{ },
};
MODULE_DEVICE_TABLE(of, spi_protocol_generic_of_match);


// (2) finding a match from stripped device-tree (no vendor part)
static const struct spi_device_id spi_protocol_generic_device_id[] = {
	{ "pro-mini-spi-generic", 0 },
	{ },
};
MODULE_DEVICE_TABLE(spi, spi_protocol_generic_device_id);
#endif /* CONFIG_OF */

static struct spi_message *
__make_reg_message(struct spi_transfer *k_xfer)
{
	struct spi_message *msg;
	msg = kmalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (msg == NULL)
		return ERR_PTR(-ENOMEM);

	spi_message_init_with_transfers(msg, k_xfer, n_reg_xfers);
	return msg;
}

static void
__make_req_reg(struct spi_transfer **xfer_pp)
{
	struct spi_transfer *xfer = *xfer_pp;
	xfer->tx_buf = &request_registry;
	xfer->rx_buf = NULL;
	//xfer->delay_usecs = 1000;
	(*xfer_pp)++;
}

static void
__make_req_cmd(struct spi_transfer **xfer_pp, u8 cmd)
{
	struct spi_transfer *xfer = *xfer_pp;
	xfer->rx_buf = NULL;
	//xfer->delay_usecs = 1000;
	switch (cmd) {
	case SET_REGISTER:
		xfer->tx_buf = &set_register;
		break;
	case GET_REGISTER:
		xfer->tx_buf = &get_register;
		break;
	}
	(*xfer_pp)++;
}

static struct spi_transfer *
__make_set_reg_transfers(struct meta_information *meta_info,
			 struct register_info *reg_info)
{
	struct spi_transfer *xfer, *head;
	int i;

	xfer = kcalloc(4, sizeof(struct spi_transfer), GFP_KERNEL);
	if (xfer == NULL)
		return ERR_PTR(-ENOMEM);

	head = xfer;

	__make_req_reg(&xfer);
	__make_req_cmd(&xfer, SET_REGISTER);

	for (i = 0; i < 4; i++)
		(head + i)->len = 1;

	xfer->tx_buf = &(reg_info->reg_addr);
	xfer->rx_buf = NULL;
	//xfer->delay_usecs = 1000;

	xfer++;

	xfer->tx_buf = &(reg_info->value);
	xfer->rx_buf = NULL;

	return head;
}

static struct spi_transfer *
__make_get_reg_transfers(struct meta_information *meta_info,
			 struct register_info *reg_info)
{
	struct spi_transfer *xfer, *head;
	int i;

	xfer = kcalloc(4, sizeof(struct spi_transfer), GFP_KERNEL);
	if (xfer == NULL)
		return ERR_PTR(-ENOMEM);

	head = xfer;

	for (i = 0; i < 4; i++)
		(head + i)->len = 1;

	__make_req_reg(&xfer);
	__make_req_cmd(&xfer, GET_REGISTER);

	xfer->tx_buf = &(reg_info->reg_addr);
	xfer->rx_buf = NULL;
	//xfer->delay_usecs = 1000;

	xfer++;

	xfer->tx_buf = NULL;
	xfer->rx_buf = &(reg_info->value);

	return head;
}

// Char subsystem functions;
static int
spi_protocol_generic_open(struct inode *inode, struct file *file_p)
{
	int retval;
        int status = -ENXIO;
	struct meta_information *meta_info;

	list_for_each_entry(meta_info, &meta_info_list, list_entry) {
		if (meta_info->device_maj_min == inode->i_rdev) {
			status = 0;
			break;
		}
	}

	if (status) {
		return status;
	}

	file_p->private_data = meta_info;

	retval = nonseekable_open(inode, file_p);
	return retval;
}

static int
spi_protocol_generic_release(struct inode *inode, struct file *file_p)
{
	file_p->private_data = NULL;
	return 0;
}

static ssize_t
spi_protocol_generic_read(struct file *file_p, char __user *buf, size_t lbuf,
		loff_t *ppos)
{
	int err;
	struct meta_information *meta_info = file_p->private_data;
	struct spi_device *__spi_device_internal = meta_info->device;
	u8 *read_buffer = kcalloc(2, sizeof(u8), GFP_KERNEL);
	struct spi_transfer read_arduino[1] = {};

	debug_printk("read() called...");

	if (lbuf > 2)
		return -EMSGSIZE;

	read_arduino[0].tx_buf = NULL;
	read_arduino[0].rx_buf = read_buffer;
	read_arduino[0].len    = 2;

	err = spi_sync_transfer(__spi_device_internal, read_arduino,
			  ARRAY_SIZE(read_arduino));
	if (err)
		return err;

	err = copy_to_user(buf, read_buffer, 2);
	if (err != 0)
		return err;
	else
		return 2;
}

static ssize_t
spi_protocol_generic_write(struct file *file_p, const char __user *buf,
			   size_t lbuf, loff_t *ppos)
{
	int err;
	u8 *write_buffer = kcalloc(2, sizeof(u8), GFP_KERNEL);
	struct spi_transfer write_arduino[1] = {};

	struct meta_information *meta_info = file_p->private_data;
	struct spi_device *__spi_device_internal = meta_info->device;

	debug_printk("write() %d bytes called...",
	       lbuf);

	if (lbuf > 2)
		return -EMSGSIZE;

	err = copy_from_user(write_buffer, buf, 2);
	if (err != 0)
		return -EFAULT;

	printk(KERN_WARNING "spi-protocol-generic: copy from us to ks ok");

	write_arduino[0].tx_buf = write_buffer;
	write_arduino[0].rx_buf = NULL;
	write_arduino[0].len	= 2;

	err = spi_sync_transfer(__spi_device_internal, write_arduino, 1);
	if (err)
		return err;
	else
		return 2;
}

static long
spi_protocol_generic_ioctl(struct file *file_p, unsigned int cmd,
			   unsigned long arg)
{
	int retval = 0;
	int status = 0;

	struct register_info tmp;
	struct spi_transfer *spi_xfers;
	struct spi_message *spi_msg;

	struct meta_information *meta_info = file_p->private_data;
	struct spi_device *__spi_device_internal = meta_info->device;

	switch (cmd) {
	case SPI_GENERIC_SET_STATUS:
		retval = copy_from_user(&tmp,
					(struct register_info __user *)arg,
					sizeof(struct register_info));

		if (retval == 0) {
			spi_xfers = __make_set_reg_transfers(meta_info, &tmp);
			if (IS_ERR(spi_xfers)) {
				debug_printk("error in making spi_xfers!");
				retval = PTR_ERR(spi_xfers);
				break;
			}

			spi_msg = __make_reg_message(spi_xfers);
			if (IS_ERR(spi_msg)) {
				debug_printk("error in making spi_msg!");
				retval = PTR_ERR(spi_msg);
				break;
			}

			status = spi_sync(__spi_device_internal, spi_msg);

			kfree(spi_msg);
			kfree(spi_xfers);

			if (status == 0)
				return 1;
			else
				return status;
		}
		break;
	case SPI_GENERIC_GET_STATUS:
		retval = copy_from_user(&tmp,
					(struct register_info __user *)arg,
					sizeof(struct register_info));

		if (retval == 0) {
			spi_xfers = __make_get_reg_transfers(meta_info, &tmp);
			if (IS_ERR(spi_xfers)) {
				debug_printk("error while making reg xfer");
				retval = PTR_ERR(spi_xfers);
				break;
			}

			spi_msg = __make_reg_message(spi_xfers);
			if (IS_ERR(spi_msg)) {
				debug_printk("error while making reg msg");
				retval = PTR_ERR(spi_msg);
				break;
			}

			status = spi_sync(__spi_device_internal, spi_msg);
			//struct spi_transfer xfer;
			//xfer.tx_buf = spi_xfers->tx_buf;
			//debug_printk("buf %x",
			//	     *((u8 *)(xfer.tx_buf)));
			//xfer.speed_hz = 100000;
			//spi_sync_transfer(__spi_device_internal, &xfer, 1);
			debug_printk("answ byte %x",
				     *((u8 *)((spi_xfers + 3)->rx_buf)));
			kfree(spi_msg);
			kfree(spi_xfers);

			if (status == 0) {
				debug_printk("from deviee: reg/val: %x/%x",
					     tmp.reg_addr, tmp.value);
				retval = copy_to_user(
					(struct register_info __user *)arg,
					&tmp,
					sizeof(struct register_info));
			}
			else
				return status;
		}
		break;
	}
	return retval;
}

static const struct file_operations spi_protocol_generic_fops = {
	.owner		= THIS_MODULE,
	.write		= spi_protocol_generic_write,
	.read		= spi_protocol_generic_read,
	.unlocked_ioctl = spi_protocol_generic_ioctl,
	.open		= spi_protocol_generic_open,
	.release	= spi_protocol_generic_release,
	.llseek		= no_llseek,
};

static int spi_protocol_generic_probe(struct spi_device *spi)
{
	struct meta_information *device_meta_info;
	int err;

	dev_t new_device_dev_t;

	struct device *sysfs_dev;

	device_meta_info = kzalloc(sizeof(struct meta_information), GFP_KERNEL);
	if (!device_meta_info)
		return -ENOMEM;

	device_meta_info->device = spi;
	device_meta_info->tx_buffer_reg = kzalloc(buf_reg_size, GFP_KERNEL);
	device_meta_info->rx_buffer_reg = kzalloc(buf_reg_size, GFP_KERNEL);

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	err = spi_setup(spi);

	if (err < 0) {
		debug_printk("spi_setup failed!");
		kfree(device_meta_info);
		return err;
	}

	debug_printk("spi_setup ok, cs: %d",
	       spi->chip_select);

	// BOO! Dark code...
	// TODO: Look through it

	// create char device entry in sysfs...
	if (first_dev_t == 0) {
		err = alloc_chrdev_region(&first_dev_t,
					  minorcount++, 1,
					  SPI_PROTOCOL_GENERIC_DEVICE);
		new_device_dev_t = first_dev_t;
	} else {
		new_device_dev_t = first_dev_t + minorcount++;
		err = register_chrdev_region(new_device_dev_t, 1,
					     SPI_PROTOCOL_GENERIC_DEVICE);
	}

	if (err < 0) {
		kfree(device_meta_info);
		return err;
	}

	device_meta_info->internal_cdev = cdev_alloc();
	if (device_meta_info->internal_cdev == NULL) {
		kfree(device_meta_info);
		unregister_chrdev_region(first_dev_t, 1);
		return -ENOMEM;
	}

	cdev_init(device_meta_info->internal_cdev, &spi_protocol_generic_fops);
	err = cdev_add(device_meta_info->internal_cdev, new_device_dev_t, 1);
	if(err < 0) {
		kfree(device_meta_info);
		unregister_chrdev_region(new_device_dev_t, 1);
		cdev_del(device_meta_info->internal_cdev);
		return err;
	}

	sysfs_dev = device_create(spi_protocol_generic_class, NULL,
				  new_device_dev_t, device_meta_info,
				  "%s.%d",
				  SPI_PROTOCOL_GENERIC_DEVICE,
				  MINOR(new_device_dev_t));
	if (IS_ERR(sysfs_dev)) {
		kfree(device_meta_info);
		unregister_chrdev_region(new_device_dev_t, 1);
		cdev_del(device_meta_info->internal_cdev);
		return PTR_ERR(sysfs_dev);
	}

	//

	device_meta_info->device_maj_min = new_device_dev_t;
	list_add(&device_meta_info->list_entry, &meta_info_list);
	spi_set_drvdata(spi, device_meta_info);

	return 0;
}

static int spi_protocol_generic_remove(struct spi_device *spi)
{
	struct meta_information *device_meta_info = spi_get_drvdata(spi);

	debug_printk("remove().");

	list_del(&device_meta_info->list_entry);
	device_destroy(spi_protocol_generic_class,
		       device_meta_info->device_maj_min);

	kfree(device_meta_info->tx_buffer_reg);
	kfree(device_meta_info->rx_buffer_reg);

	// BOO! Dark code...
	// TODO: LOOK THROUGH IT!

	cdev_del(device_meta_info->internal_cdev);
	unregister_chrdev_region(device_meta_info->device_maj_min, 1);
	kfree(device_meta_info);

	//
	return 0;
}

static struct spi_driver spi_protocol_generic = {
	.driver = {
		.owner		= THIS_MODULE,
		.name		= "spi-protocol-generic",
		.of_match_table = of_match_ptr(spi_protocol_generic_of_match),
	},
	.probe	= spi_protocol_generic_probe,
	.remove = spi_protocol_generic_remove,
};

static int __init spi_protocol_generic_init(void)
{
	int status;

	minorcount = 0;

	// define a device class
	spi_protocol_generic_class = class_create(THIS_MODULE, SPI_CLASS_NAME);
	if (IS_ERR(spi_protocol_generic_class)) {
		debug_printk("class_create failed!");
		spi_unregister_driver(&spi_protocol_generic);
		return PTR_ERR(spi_protocol_generic_class);
	}

	status = spi_register_driver(&spi_protocol_generic);
	if (status < 0)
		return status;

	return status;
}

static void __exit spi_protocol_generic_exit(void)
{
	spi_unregister_driver(&spi_protocol_generic);
	class_destroy(spi_protocol_generic_class);
}

module_init(spi_protocol_generic_init);
module_exit(spi_protocol_generic_exit);

// module_spi_driver(spi_protocol_generic);
MODULE_DESCRIPTION("Generic SPI driver for echo device");
MODULE_AUTHOR("Georgiy Odisharia");
MODULE_LICENSE("GPL");
