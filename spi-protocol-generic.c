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
#endif // CONFIG_OF

static int spi_protocol_generic_probe(struct spi_device *spi) {
    int err;
    const struct of_device_id *match;
    unsigned char ch16[] = {0x5A, 0x5A};    
    unsigned char rx16[] = {0x00, 0x00};
    int devData = 0;
    printk("mySPI_slave::my_device_probe called.\n");

    // check and read data from of_device_id...
    match = of_match_device(spi_protocol_generic_of_match, &spi->dev);
    if(!match) {
        printk("mySPI_slave::my_device_probe drvice not found in device tree...\n");
    } 
    else {
        devData = match->data;
        printk("mySPI_slave::my_device_probe data is: %d\n", devData);
    }


    spi->bits_per_word = 8;
    spi->mode = (0);
    
    err = spi_setup(spi);
    if (err < 0) {
            printk("mySPI_slave::my_device_probe spi_setup failed!\n");
        return err;
    }

    printk("spi_setup ok, cs: %d\n", spi->chip_select);
    printk("start data transfer...\n");

    struct spi_transfer spi_element[] = {
        {
            .len = 2,
            .cs_change = 0,
        }, 
        {
            .len = 2,
        },
    };

    spi_element[0].tx_buf = ch16;
    spi_element[1].rx_buf = rx16;

    err = spi_sync_transfer(spi, spi_element, ARRAY_SIZE(spi_element));
    printk("data size: %d\n", sizeof(rx16));
    if (err < 0) {
        printk("mySPI_slave::my_device_probe spi_sync_transfer failed!\n");
        return err;
    }

    printk("transfer ok\n");
}

static int spi_protocol_generic_remove(struct spi_device *spi)
{
    printk("my_device_remove() called.\n");
    return 0;
}

static struct spi_driver spi_protocol_generic = {
    .driver = {
        .owner      =  THIS_MODULE,
        .name       = "spi-protocol-generic",
        .of_match_table = of_match_ptr(spi_protocol_generic_of_match),
    },
    .probe  = spi_protocol_generic_probe,
    .remove = spi_protocol_generic_remove,  
};

module_spi_driver(spi_protocol_generic);
MODULE_DESCRIPTION("Generic SPI driver for echo device");
MODULE_AUTHOR("Georgiy Odisharia");
MODULE_LICENSE("GPL");
