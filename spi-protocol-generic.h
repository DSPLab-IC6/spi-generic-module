#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#endif

#define SPI_GENERIC_MAGIC	0xDE
#define SPI_GENERIC_SET_STATUS	_IOW( SPI_GENERIC_MAGIC, 0, struct register_info)
#define SPI_GENERIC_GET_STATUS	_IOR( SPI_GENERIC_MAGIC, 1, struct register_info)
#define DEVPATH "/dev/pro_mini_spi_generic"

struct register_info {
	uint8_t reg_addr;
	uint8_t value;
};
