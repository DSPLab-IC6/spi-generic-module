/*
 * Simple protocol driver for generic SPI device.
 * Targeted to communcate with Arduino, but nowdays
 * supports only loopback mode
 *
 * Copyright (C) 2017 Georgiy Odisharia 
 * 	<math.kraut.cat@gmail.com>
 *
 * This program is distributed under GPLv2 license.
 * See LICENSE.md for morre information.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <linux/spi.h>

static struct spi_driver spi_protocol_generic = {
	.driver 	= {
		.name 		= "spi-protocol-generic",
		.of_match_table = if
	},
}

module_spi_driver(spi_protocol_generic);
MODULE_DESCRIPTION("Generic SPI driver for echo device");
MODULE_AUTHOR("Goergiy Odisharia");
MODULE_LICENSE("GPL");
