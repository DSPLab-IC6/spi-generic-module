/*
 * Simple protocol driver for generic SPI device.
 * Targeted to communcate with Arduino, but nowdays
 * supports only loopback mode
 *
 * Copyright (C) 2017 Georgiy Odisharia
 *  <math.kraut.cat@gmail.com>
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
#include <linux/spi/spi.h>

#include <linux/fs.h>
#include <linux/cdev.h>


#define SPI_CLASS_NAME 					"spi_protocol_generic"
#define SPI_PROTOCOL_GENERIC_DEVICE_0 	"pro_mini_spi_generic"

static dev_t spi_protocol_generic_dev_t;
static struct device        *spi_protocol_generic_dev;
static struct cdev          *spi_protocol_generic_cdev;
static struct class         *spi_protocol_generic_class;
static struct spi_device    *spi_protocol_generic_spi;

struct spi_device_default_values {
    u32     speed_hz;
    u16     mode;
    u8      bits_per_word;
};

static struct spi_device *__spi_device_internal;
static struct spi_device_default_values default_values;

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

// Char subsystem functions;
static int spi_protocol_generic_open(struct inode *inode, struct file *file_p)
{
}

static int spi_protocol_generic_release(struct inode *inode, struct file *file_p)
{
}

static ssize_t
spi_protocol_generic_read(struct file *file_p, char __user *buf, size_t lbuf,
			  loff_t *ppos)
{
    int err;
    u8 *read_buffer;

    printk(KERN_DEBUG "spi-protocol-generic: read() called...\n");

    struct spi_transfer read_arduino[1];

    read_arduino[0].tx_buf = NULL;
    read_arduino[0].rx_buf = read_buffer;

    spi_sync_transfer(__spi_device_internal, read_arduino, ARRAY_SIZE(read_arduino));

    err = copy_to_user(buf, read_buffer, 2);
    if (err < 0) {
        return err;
    } else {
        return 2;
    }
}

static ssize_t
spi_protocol_generic_write(struct file *file_p, const char __user *buf,
			   size_t lbuf, loff_t *ppos)
{

    int err;
    u8 *write_buffer = kmalloc(2, GFP_KERNEL);

    printk(KERN_DEBUG "spi-protocol-generic: write() %d bytes called...\n", lbuf);

    struct spi_transfer write_arduino[1];

    if (lbuf > 2)
        return -EMSGSIZE;

    err = copy_from_user(write_buffer, buf, 2);

    if (err != 0)
        return -EFAULT;

    write_arduino[0].tx_buf = write_buffer;
    write_arduino[0].rx_buf = NULL;

    err = spi_sync_transfer(__spi_device_internal, write_arduino, 1);
    if (err)
        return err;
    else
        return 2;
}

static const struct file_operations spi_protocol_generic_fops = {
    .owner      = THIS_MODULE,
    .write      = spi_protocol_generic_write,
    .read       = spi_protocol_generic_read,
    .open       = spi_protocol_generic_open,
    .release    = spi_protocol_generic_release,
};

static int spi_protocol_generic_probe(struct spi_device *spi)
{
    int err;

    const struct of_device_id *match;
	int devData = 0;

    int max_speed_arduino = 20000;

    unsigned char ch16[] = {0xDE, 0xAD};
    unsigned char *rx16 = kzalloc(2, GFP_KERNEL);

	printk(KERN_DEBUG "spi-protocol-generic: probe called.\n");

    __spi_device_internal = spi;
    default_values.speed_hz         = spi->max_speed_hz;
    default_values.mode             = spi->mode;
    default_values.bits_per_word    = spi->bits_per_word;

    printk(KERN_DEBUG "spi-protocol-generic: default speed is %d Hz\n", default_values.speed_hz);

    // check and read data from of_device_id...
    match = of_match_device(spi_protocol_generic_of_match, &spi->dev);
    if(!match) {
        printk(KERN_DEBUG "spi-protocol-generic: device not found in device tree...\n");
    }
    else {
        devData = match->data;
        printk(KERN_DEBUG "spi-protocol-generic: probe data is: %d\n", devData);
    }

    spi->bits_per_word = 8;
    spi->mode = (0);
    spi->max_speed_hz = 20000;
    spi_setup(spi);

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
    printk(KERN_DEBUG "spi-protocol-generic: start data transfer...\n");

    struct spi_transfer spi_element[] = {
        {
            .len = 2,
        },
        {
            .len = 2,
        },
    };

    spi_element[0].tx_buf = ch16;
    spi_element[0].speed_hz = 20000;
    spi_element[1].rx_buf = rx16;
    spi_element[1].speed_hz = 20000;

    err = spi_sync_transfer(__spi_device_internal, spi_element,
			    ARRAY_SIZE(spi_element));
    printk(KERN_DEBUG "spi-protocol-generic: data size is %d\n", 2);
    if (err < 0) {
        printk(KERN_DEBUG "spi-protocol-generic: spi_sync_transfer failed!\n");
        return err;
    }

    printk(KERN_DEBUG "spi-protocol-generic: transfer ok\n");
    printk("%X %X\n", rx16[0], rx16[1]);
    print_hex_dump_bytes("", DUMP_PREFIX_NONE, rx16, 2);

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

    err = cdev_add(spi_protocol_generic_cdev, spi_protocol_generic_dev_t, 1);
    if(err < 0) {
        printk(KERN_DEBUG "spi_protocol_generic: cdev_add failed!\n");
        cdev_del(spi_protocol_generic_cdev);
        unregister_chrdev_region(spi_protocol_generic_dev_t, 1);
        class_destroy(spi_protocol_generic_class);
        return err;
    }

    if (devData == 0) {
        spi_protocol_generic_dev = device_create(spi_protocol_generic_class,
						 NULL,
						 spi_protocol_generic_dev_t,
						 NULL,
						 "%s",
						 SPI_PROTOCOL_GENERIC_DEVICE_0);
    }

    return 0;
}

static int spi_protocol_generic_remove(struct spi_device *spi)
{
    printk(KERN_DEBUG "spi-protocol-generic: remove().\n");

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
        .owner          = THIS_MODULE,
        .name           = "spi-protocol-generic",
        .of_match_table = of_match_ptr(spi_protocol_generic_of_match),
    },
    .probe  = spi_protocol_generic_probe,
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
