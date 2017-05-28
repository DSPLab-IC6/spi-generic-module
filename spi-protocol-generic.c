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
	printk(KERN_CONT __VA_ARGS__)
#else /* DEBUG_SPI_PROTOCOL_GENERIC */
#define debug_printk(...) do {} while(0)
#endif /* DEBUG_SPI_PROTOCOL_GENERIC */

#define SPI_CLASS_NAME			"spi_protocol_generic"
#define SPI_PROTOCOL_GENERIC_DEVICE_0	"pro_mini_spi_generic"

#define MAGIX_TRANSACTION 0xFA
#define GET_REGISTER	  0xDE
#define SET_REGISTER      0xDB

static u8 request_registry = MAGIX_TRANSACTION;
static u8 get_register	   = GET_REGISTER;
static u8 set_register     = SET_REGISTER;

static dev_t spi_protocol_generic_dev_t;
static struct device		*spi_protocol_generic_dev;
static struct cdev		*spi_protocol_generic_cdev;
static struct class		*spi_protocol_generic_class;

struct spi_device_default_values {
	u32 speed_hz;
	u16 mode;
	u8  bits_per_word;
};

static struct spi_device *__spi_device_internal;
static struct spi_device_default_values default_values;

static u8 *tx_buffer_reg;
static u8 *rx_buffer_reg;
static unsigned buf_reg_size = 2;
static unsigned n_reg_xfers = 2;

#ifdef CONFIG_OF
static const struct of_device_id spi_protocol_generic_of_match[] = {
	{ .compatible = "arduino,pro-mini-spi-generic" },
	{ },
};
MODULE_DEVICE_TABLE(of, spi_protocol_generic_of_match);


// (2) finding a match from stripped device-tree (no vendor part)
static const struct spi_device_id spi_protocol_generic_device_id[] = {
	{ "pro-mini-spi-generic", 0 },
	{ },
};
MODULE_DEVICE_TABLE(spi, spi_protocol_generic_device_id);
#endif // CONFIG_OF

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
__make_req_reg(struct spi_transfer *xfer)
{
	xfer->tx_buf = &request_registry;
	xfer->rx_buf = NULL;
	xfer->delay_usecs = 1;
}

static void
__make_req_cmd(struct spi_transfer *xfer, u8 cmd)
{
	xfer->rx_buf = NULL;
	xfer->delay_usecs = 1;
	switch (cmd) {
	case SET_REGISTER:
		xfer->tx_buf = &set_register;
		break;
	case GET_REGISTER:
		xfer->tx_buf = &get_register;
		break;
	}
}

static struct spi_transfer *
__make_reg_transfers(struct register_info *reg_info)
{
	struct spi_transfer *xfer, *head;
	u8 *tx_buf;
	unsigned n;

	// Copy to global TX buffer values of reg_info fields
	*tx_buffer_reg = reg_info->reg_addr;
	*(tx_buffer_reg + 1) = reg_info->value;

	tx_buf = tx_buffer_reg;

	xfer = kcalloc(4, sizeof(struct spi_transfer), GFP_KERNEL);
	if (xfer == NULL)
		return ERR_PTR(-ENOMEM);

	head = xfer;

	__make_req_reg(xfer++);
	__make_req_cmd(xfer++, SET_REGISTER);

	for (n = n_reg_xfers;
	     n;
	     n--, xfer++, tx_buf++) {
		xfer->tx_buf = tx_buf;
		xfer->rx_buf = NULL;
		xfer->delay_usecs = 1;
	}

	return head;
}

static struct spi_transfer *
__make_get_reg_transfers(struct register_info *reg_info)
{
	struct spi_transfer *xfer, *head;

	xfer = kcalloc(4, sizeof(struct spi_transfer), GFP_KERNEL);
	if (xfer == NULL)
		return ERR_PTR(-ENOMEM);

	head = xfer;

	__make_req_reg(xfer++);
	__make_req_cmd(xfer++, GET_REGISTER);

	xfer->tx_buf = &(reg_info->reg_addr);
	xfer->rx_buf = NULL;
	xfer->delay_usecs = 1;

	xfer++;

	xfer->tx_buf = NULL;
	xfer->rx_buf = &(reg_info->value);

	return head;
}

// Char subsystem functions;
static int spi_protocol_generic_open(struct inode *inode, struct file *file_p)
{
	int retval;
	retval = nonseekable_open(inode, file_p);
	return retval;
}

static int spi_protocol_generic_release(struct inode *inode,
					struct file *file_p)
{
	return 0;
}

static ssize_t
spi_protocol_generic_read(struct file *file_p, char __user *buf, size_t lbuf,
		loff_t *ppos)
{
	int err;
	u8 *read_buffer = kcalloc(2, sizeof(u8), GFP_KERNEL);
	struct spi_transfer read_arduino[1] = {};

	printk(KERN_DEBUG "spi-protocol-generic: read() called...\n");

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

	printk(KERN_DEBUG "spi-protocol-generic: write() %d bytes called...\n",
	       lbuf);

	if (lbuf > 2)
		return -EMSGSIZE;

	err = copy_from_user(write_buffer, buf, 2);
	if (err != 0)
		return -EFAULT;

	printk(KERN_WARNING "spi-protocol-generic: copy from us to ks ok\n");

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

	switch (cmd) {
	case SPI_GENERIC_SET_STATUS:
		retval = copy_from_user(&tmp,
					 (struct register_info __user *)arg,
					 sizeof(struct register_info));
		if (retval == 0) {
			spi_xfers = __make_reg_transfers(&tmp);
			if (IS_ERR(spi_xfers)) {
				retval = PTR_ERR(spi_xfers);
				break;
			}

			spi_msg = __make_reg_message(spi_xfers);
			if (IS_ERR(spi_msg)) {
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
			spi_xfers = __make_get_reg_transfers(&tmp);
			if (IS_ERR(spi_xfers)) {
				retval = PTR_ERR(spi_xfers);
				break;
			}

			spi_msg = __make_reg_message(spi_xfers);
			if (IS_ERR(spi_msg)) {
				retval = PTR_ERR(spi_msg);
				break;
			}

			status = spi_sync(__spi_device_internal, spi_msg);
			kfree(spi_msg);
			kfree(spi_xfers);

			if (status == 0) {
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
	int err, ret;
	int devData = 0;
	const struct of_device_id *match;

	struct device_node *of_target_node, *parent;
	u32 max_speed_arduino = 0;

	const char *of_compt_str = kcalloc(sizeof(char), 255, GFP_KERNEL);

	printk(KERN_DEBUG "spi-protocol-generic: probe called.\n");

	__spi_device_internal = spi;
	default_values.speed_hz = spi->max_speed_hz;
	default_values.mode = spi->mode;
	default_values.bits_per_word = spi->bits_per_word;

	printk(KERN_DEBUG "spi-protocol-generic: default speed is %d Hz\n",
	       default_values.speed_hz);

	// check and read data from of_device_id...
	match = of_match_device(spi_protocol_generic_of_match, &spi->dev);
	if (!match) {
		debug_printk("device not found in device tree...\n");
		return -1;
	}
	of_target_node = of_find_compatible_node(NULL, NULL, match->compatible);
	if (!of_target_node) {
		of_node_put(of_target_node);
		debug_printk("no compatible devices in DT!\n");
	}
	parent = of_target_node;
	// Here we can use of_get_property, but I prefer more readable
	// code insted of mire obvious
	ret = of_property_read_u32(of_target_node, "spi-max-frequency",
			   &max_speed_arduino);
	if (ret) {
		debug_printk("cannot find property in DT node, status %d\n",
			     ret);
		return ret;
	} else {
		debug_printk("freq is %d Hz\n", max_speed_arduino);
	}

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = max_speed_arduino;
	err = spi_setup(spi);

	if (err < 0) {
		printk(KERN_DEBUG "spi-protocol-generic: spi_setup failed!\n");
		return err;
	} else {
		printk(KERN_DEBUG "spi-protocol-generic: now speed is %d Hz\n",
		       spi->max_speed_hz);
	}

	printk(KERN_DEBUG "spi-protocol-generic: spi_setup ok, cs: %d\n",
	       spi->chip_select);

	// define a device class
	spi_protocol_generic_class = class_create(THIS_MODULE, SPI_CLASS_NAME);
	if (spi_protocol_generic_class == NULL) {
		printk(KERN_DEBUG "spi_protocol_generic: class_create failed!\n");
		return -1;
	}

	// create char device entry in sysfs...
	if (devData == 0) {
		err = alloc_chrdev_region(&spi_protocol_generic_dev_t, 0, 1,
		  SPI_PROTOCOL_GENERIC_DEVICE_0);
	}
	if (err < 0) {
		printk(KERN_DEBUG "spi_protocol_generic: alloc_chrdev_region failed!\n");
		class_destroy(spi_protocol_generic_class);
		return err;
	}

	spi_protocol_generic_cdev = cdev_alloc();
	if (!(spi_protocol_generic_cdev)) {
		printk(KERN_DEBUG "spi_protocol_generic: cdev_alloc failed!\n");
		unregister_chrdev_region(spi_protocol_generic_dev_t, 1);
		class_destroy(spi_protocol_generic_class);
		return -1;
	}

	cdev_init(spi_protocol_generic_cdev, &spi_protocol_generic_fops);

	err = cdev_add(spi_protocol_generic_cdev,
		       spi_protocol_generic_dev_t, 1);
	if(err < 0) {
		printk(KERN_DEBUG "spi_protocol_generic: cdev_add failed!\n");
		cdev_del(spi_protocol_generic_cdev);
		unregister_chrdev_region(spi_protocol_generic_dev_t, 1);
		class_destroy(spi_protocol_generic_class);
		return err;
	}

	if (devData == 0) {
		spi_protocol_generic_dev =
			device_create(spi_protocol_generic_class, NULL,
				      spi_protocol_generic_dev_t, NULL, "%s",
				      SPI_PROTOCOL_GENERIC_DEVICE_0);
	}

	// Initiate buffer for registry operations
	tx_buffer_reg = kzalloc(buf_reg_size, GFP_KERNEL);
	rx_buffer_reg = kzalloc(buf_reg_size, GFP_KERNEL);
	kfree(of_compt_str);

	return 0;
}

static int spi_protocol_generic_remove(struct spi_device *spi)
{
	printk(KERN_DEBUG "spi-protocol-generic: remove().\n");

	kfree(tx_buffer_reg);
	kfree(rx_buffer_reg);

	spi->max_speed_hz = default_values.speed_hz;
	spi_setup(spi);

	device_destroy(spi_protocol_generic_class, spi_protocol_generic_dev_t);
	if(spi_protocol_generic_cdev) {
		cdev_del(spi_protocol_generic_cdev);
	}
	unregister_chrdev_region(spi_protocol_generic_dev_t, 1);
	class_destroy(spi_protocol_generic_class);
	return 0;
}

static struct spi_driver spi_protocol_generic = {
	.driver = {
		.owner			= THIS_MODULE,
		.name			= "spi-protocol-generic",
		.of_match_table = of_match_ptr(spi_protocol_generic_of_match),
	},
	.probe	= spi_protocol_generic_probe,
	.remove = spi_protocol_generic_remove,
};

static int __init spi_protocol_generic_init(void) {
	int status;
	status = spi_register_driver(&spi_protocol_generic);
	return status;
}

static void __exit spi_protocol_generic_exit(void) {
	spi_unregister_driver(&spi_protocol_generic);
}

module_init(spi_protocol_generic_init);
module_exit(spi_protocol_generic_exit);

// module_spi_driver(spi_protocol_generic);
MODULE_DESCRIPTION("Generic SPI driver for echo device");
MODULE_AUTHOR("Georgiy Odisharia");
MODULE_LICENSE("GPL");
