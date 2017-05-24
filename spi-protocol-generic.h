#include <linux/ioctl.h>

#define SPI_GENERIC_MAGIC	0xDE
#define SPI_GENERIC_SET_STATUS	_IOW( SPI_GENERIC_MAGIC, 0, struct register_info)
#define SPI_GENERIC_GET_STATUS	_IOR( SPI_GENERIC_MAGIC, 1, struct register_info)
#define DEVPATH "/dev/pro_m"

struct register_info {
	__u8 reg_addr;
	__u8 value;
};
