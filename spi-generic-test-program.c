#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "spi-protocol-generic.h"

#define ERR(...) printf("Oooops! "); \
        fprintf(stderr, "\7" __VA_ARGS__); \
        exit(EXIT_FAILURE)

int main( int argc, char *argv[] ) {
    int dfd, ret;                                    // дескриптор устройства 
    struct register_info regs[2];
    char *NEWPATH[2];
    for (int i = 0; i < 1; i++) {
        NEWPATH[i] = malloc(sizeof(char) * (sizeof(DEVPATH) + 2));
        strncpy(NEWPATH[i], DEVPATH, sizeof(DEVPATH) + 2);
        snprintf(NEWPATH[i], sizeof(DEVPATH) + 2, "%s.%d", DEVPATH, i);
    }
    regs[0].reg_addr = 0x01;
    regs[0].value    = 0xDF;
    regs[1].reg_addr = 0x01;

    for (int i = 0; i < 1; i++) {
        dfd = open(NEWPATH[i], O_RDWR);
//        if (dfd < 0) 
//            ERR("Open device error: %m\n");
        
        ret = ioctl(dfd, SPI_GENERIC_SET_STATUS, &regs[0]);
//        if (ret != 1)
//            ERR("ioctl() SET_STATUS error: %m\n");
        
        ret = ioctl(dfd, SPI_GENERIC_GET_STATUS, &regs[1]);
//        if (ret != 0)
//            ERR("ioctl() GET_STATUS error: %m\n");
        printf("value is %x\n", regs[1].value);
    }

    close(dfd);
    return EXIT_SUCCESS;
};
