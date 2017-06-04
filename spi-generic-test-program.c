#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include "spi-protocol-generic.h"

#define ERR(...) fprintf(stderr, "\7" __VA_ARGS__), exit(EXIT_FAILURE)

int main( int argc, char *argv[] ) {
    int dfd;                                    // дескриптор устройства 
    struct register_info
    if ((dfd = open(DEVPATH, O_RDWR)) < 0) 
        ERR( "Open device error: %m\n" );
    if (ioctl(dfd, SPI_GENERIC_SET_STATUS, &buf)) 
        ERR("IOCTL_GET_STRING error: %m\n");
    fprintf(stdout, (char *)&buf);
    close(dfd);
    return EXIT_SUCCESS;
};
