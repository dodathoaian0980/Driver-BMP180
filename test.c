#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h> 

#define DEVICE_PATH "/dev/bmp180"
#define OSS 3

#define BMP180_IOCTL_MAGIC 'b'
#define BMP180_IOCTL_TEMPERATURE _IOR(BMP180_IOCTL_MAGIC,1, int)
#define BMP180_IOCTL_PRESSURE _IOR(BMP180_IOCTL_MAGIC,2, int)
#define BMP180_IOCTL_SET_OSS _IOW(BMP180_IOCTL_MAGIC,3, int) 

int main() {
    int fd;
    int data;
    int oss=OSS;
    // Open the device
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return errno;
    }
// choose your oversampling_setting 
    if (ioctl(fd, BMP180_IOCTL_SET_OSS, &oss) < 0) {
        perror("Failed to set oss data");
        return errno;
    }

    if (ioctl(fd, BMP180_IOCTL_TEMPERATURE, &data) < 0) {
        perror("Failed to calculate temperatuere data");
        close(fd);
        return errno;
    }
    printf("Temperature value: %d (C)\n", data/10);

    if (ioctl(fd, BMP180_IOCTL_PRESSURE, &data) < 0) {
        perror("Failed to calculate pressure data");
        close(fd);
        return errno;
    }  
    printf("Pressure value: %d (Pa)\n", data);

    // Close the device
    close(fd);
    return 0;
}
