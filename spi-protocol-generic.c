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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>

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
spi_protocol_generic_read(struct file *file_p, char __user *buf, size_t lbuf, loff_t *ppos)
{
}

static ssize_t
spi_protocol_generic_write(struct file *file_p, const char __user *buf, size_t lbuf, loff_t *ppos)



static const struct file_operations spi_protocol_generic_fops = {
    .owner      = THIS_MODULE,
    .write      = spi_protocol_generic_write,
    .read       = spi_protocol_generic_read,
    .open       = spi_protocol_generic_open,
    .release    = spi_protocol_generic_release,
};

static int spi_protocol_generic_probe(struct spi_device *spi) {
    int err;
    const struct of_device_id *match;
    unsigned char ch16[] = {0xDE, 0xAD};    
    unsigned char *rx16 = kzalloc(2, GFP_KERNEL);
    int devData = 0;
    printk("spi-protocol-generic: probe called.\n");

    // check and read data from of_device_id...
    match = of_match_device(spi_protocol_generic_of_match, &spi->dev);
    if(!match) {
        printk("spi-protocol-generic: device not found in device tree...\n");
    } 
    else {
        devData = match->data;
        printk("spi-protocol-generic: probe data is: %d\n", devData);
    }

    spi->bits_per_word = 8;
    spi->mode = (0);
    
    err = spi_setup(spi);
    if (err < 0) {
        printk("spi-protocol-generic: spi_setup failed!\n");
        return err;
    }

    printk("spi-protocol-generic: spi_setup ok, cs: %d\n", spi->chip_select);
    printk("spi-protocol-generic: start data transfer...\n");

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

    err = spi_sync_transfer(spi, spi_element, ARRAY_SIZE(spi_element));
    printk("spi-protocol-generic: data size is %d\n", 2);
    if (err < 0) {
        printk("spi-protocol-generic: spi_sync_transfer failed!\n");
        return err;
    }

    printk("spi-protocol-generic: transfer ok\n");
    printk("%X %X\n", rx16[0], rx16[1]);
    print_hex_dump_bytes("", DUMP_PREFIX_NONE, rx16, 2);

    return 0;
}

static int spi_protocol_generic_remove(struct spi_device *spi)
{
    printk("spi-protocol-generic: remove().\n");
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
