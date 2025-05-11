#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/cdev.h>

#define DRIVER_NAME "bmp180_driver"
#define CLASS_NAME "bmp180"
#define DEVICE_NAME "bmp180"


#define BMP_I2C_ADDRESS 0x77
#define BMP180_E2PROM_1_REG 0xAA
#define BMP180_CTRL_MEAS_REG 0xF4
#define BMP180_MSB_REG 0xF6
#define BMP180_CHIP_ID     0xD0
#define BMP180_TEMP_MEAS 0x2E
#define BMP180_PRESS_MEAS 0x34
#define BMP180_E2PROM_DATA_LENGTH 22
#define DELAY 5

// // IOCTL commands
#define BMP180_IOCTL_MAGIC 'b'
#define BMP180_IOCTL_TEMPERATURE _IOR(BMP180_IOCTL_MAGIC,1, int)
#define BMP180_IOCTL_PRESSURE _IOR(BMP180_IOCTL_MAGIC,2, int)
#define BMP180_IOCTL_SET_OSS _IOW(BMP180_IOCTL_MAGIC,3, int)
#define BMP180_IOCTL_ALTITUDE _IOR(BMP180_IOCTL_MAGIC,4, int)

static struct i2c_client *bmp180_client;
static struct class* bmp180_class = NULL;
static struct device* bmp180_device = NULL;
static int major_number;


struct bmp180_E2PROM_data {
    s16 AC1, AC2, AC3;
    u16 AC4, AC5, AC6;
    s16 B1, B2;
    s16 MB, MC, MD;
};

struct bmp180_data {
    struct i2c_client *client;
    struct mutex lock;
    struct mutex power_lock;
    struct bmp180_E2PROM_data calibration;
    u32 raw_temperature;
    u32 raw_pressure;
    unsigned char oversampling_setting;
    s32 B5;
    s32 B6;
};

static s32 bmp180_read_E2PROM(struct i2c_client *client)
{
    u8 buf[BMP180_E2PROM_DATA_LENGTH];
    struct bmp180_data *data = i2c_get_clientdata(client); // access to get data again
    struct bmp180_E2PROM_data *cali = &(data->calibration);
    if (i2c_smbus_read_i2c_block_data(client, BMP180_E2PROM_1_REG, BMP180_E2PROM_DATA_LENGTH,buf) < 0)
    {
        printk(KERN_ERR "Failed to read data\n");
        return -EIO;
    }
    cali->AC1 =(buf[0] << 8) | buf[1]; //AC1
    cali->AC2 = (buf[2] << 8) | buf[3]; //AC2
    cali->AC3 = (buf[4] << 8) | buf[5]; //AC3
    cali->AC4 = (buf[6] << 8) | buf[7]; //AC4
    cali->AC5 = (buf[8] << 8) | buf[9]; //AC5
    cali->AC6 = (buf[10] << 8) | buf[11]; //AC6
    cali->B1 = (buf[12] << 8) | buf[13]; //B1
    cali->B2 = (buf[14] << 8) | buf[15]; //B2
    cali->MB = (buf[16] << 8) | buf[17]; //MB
    cali->MC = (buf[18] << 8) | buf[19]; //MC
    cali->MD = (buf[20] << 8) | buf[21];//MD
    return 0;

}
/**
* Use mutex_clock function to synchronize data access,
* allowing only one process at a time to prevent race conditions
*/
static s32 bmp180_update_raw_temperature(struct bmp180_data *data)
{
    s32 status;
    u8 buf[2];
    mutex_lock(&data->lock);
    status = i2c_smbus_write_byte_data(data->client, BMP180_CTRL_MEAS_REG, BMP180_TEMP_MEAS);
    if (status != 0) {
        pr_err("Error while requesting temperature measurement.\n");
        goto exit;
    }
    msleep(DELAY);
    status = i2c_smbus_read_i2c_block_data(data->client,BMP180_MSB_REG,2,buf);
    if(status <0){
        printk(KERN_ERR "Failed to read temperature data\n");
        goto exit;
    }
    data->raw_temperature = (buf[0] << 8) | buf[1];
    status = 0; /* everything ok, return 0 */
exit:
    mutex_unlock(&data->lock);
    return status;
}

static s32 bmp180_update_raw_pressure(struct bmp180_data *data)
{
    s32 status;
    u8 buf[3];
    mutex_lock(&data->lock);
    status = i2c_smbus_write_byte_data(data->client, BMP180_CTRL_MEAS_REG, BMP180_PRESS_MEAS + (data->oversampling_setting<<6));
    if (status != 0) {
        pr_err("Error while requesting pressure measurement.\n");
        goto exit;
    }

    msleep(2 + ( 3 << data->oversampling_setting)); // Wait time is adjusted based on the oversampling setting (OSS) value

    status = i2c_smbus_read_i2c_block_data(data->client, BMP180_MSB_REG, 3, buf);
    if(status <0){
        printk(KERN_ERR "Failed to read pressure data\n");
        goto exit;
    }
    data->raw_pressure=(buf[0]<<16 | buf[1]<<8 | buf[2])>>(8 - data->oversampling_setting);
    status = 0;
exit:
    mutex_unlock(&data->lock);
    return status;
}

/*
 * This function starts the temperature measurement and returns the value
 * in tenth of a degree celsius.
 */
static s32 bmp180_get_temperature(struct bmp180_data *data, int *temperature)
{
    long X1, X2;
    int status;
    struct bmp180_E2PROM_data *cali = &data->calibration;
    status = bmp180_update_raw_temperature(data);
    if (status != 0){
        printk(KERN_ERR "Failed to update temperature value %d\n",status);
        goto exit;
    }
    X1 = ((data->raw_temperature - cali->AC6) * cali->AC5 )>> 15;
    X2 = (cali->MC << 11) / (X1 + cali->MD);

    data->B5 = X1 + X2;
    data -> B6 =data->B5-4000;

    if (temperature != NULL)
    {
        *temperature = (data->B5 + 8) >> 4;
    }
    status=0;
exit:
    return status;
}

/*
 * This function starts the pressure measurement and returns the value in Pa.
 * We should callback "temperature measurement" before starts this function.
 */
static s32 bmp180_get_pressure(struct bmp180_data *data, int *pressure)
{
    s32 X1, X2, X3, B3, p;
    u32 B4, B7;
    int status;
    struct bmp180_E2PROM_data *cali = &data->calibration;
    status = bmp180_update_raw_pressure(data);
    if (status != 0){
        printk(KERN_ERR "Failed to update pressure value %d\n",status);
        goto exit;
    }
    X1 = ( cali->B2 * (((data->B6) * data->B6) >> 12) ) >>11;
    X2 = (cali->AC2 * data->B6) >>11;
    X3 = X1 + X2;
    B3 = ( ( ( ((s32)cali->AC1) * 4 + X3) << data->oversampling_setting) + 2) /4;
    X1 = (cali->AC3 * data->B6) >> 13;
    X2 = (cali->B1 * ((data->B6 * data->B6) >> 12)) >> 16;
    X3 = (X1 + X2 + 2) >> 2;
    B4 = (cali->AC4 * (u32)(X3 + 32768)) >> 15;
    B7 = ( (u32)data->raw_pressure - B3 ) * (50000 >> data->oversampling_setting);
    if (B7 < 0x80000000)  p=(B7*2)/B4;
    else p=(B7/B4)*2; // Avoid overflow by multiplying before division
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = ( -7375 * p) >> 16;
    p += (X1 + X2 + 3791)>> 4;
    if (pressure != NULL)
    {
        *pressure=p;
    }
    status=0;
exit:
    return status;
}

/*
 * This function starts the altitude measurement and returns the value in m.
 */
static s32 bmp180_get_altitude(struct bmp180_data *data, unsigned int *altitude)
{
    const s32 P0=101325;
    s32 P=0;
    if(!data)
        return -1;
    if (bmp180_get_pressure(data, &P)!= 0)
        return -EIO;
/*this is a simplified linearized version of the barometric altitude formula */
    *altitude = (P0-P)*843/10000;
    return 0;
}

static long bmp180_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int data;
    int oss;
    struct bmp180_data *bmp_data = i2c_get_clientdata(bmp180_client);
    switch (cmd) {
        case BMP180_IOCTL_TEMPERATURE:
            bmp180_get_temperature(bmp_data, &data);
            if (copy_to_user((int __user *)arg, &data, sizeof(data)))
                return -EFAULT;
            break;
        case BMP180_IOCTL_PRESSURE:
            bmp180_get_temperature(bmp_data, NULL); // Update temperature value first before other calculations
            bmp180_get_pressure(bmp_data, &data);
            if (copy_to_user((int __user *)arg, &data, sizeof(data)))
                return -EFAULT;
            break;
        case BMP180_IOCTL_SET_OSS:
            if (copy_from_user(&oss, (int __user *)arg, sizeof(oss)))
                return -EFAULT;
            if (oss < 0 || oss > 3)
                return -EINVAL;
            bmp_data->oversampling_setting = oss;
            pr_info("Set oversampling to %d\n", oss);
            break;
        case BMP180_IOCTL_ALTITUDE:
            bmp180_get_temperature(bmp_data, NULL);
            bmp180_get_altitude(bmp_data,&data);
            if (copy_to_user((int __user *)arg, &data, sizeof(data)))
                return -EFAULT;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static int bmp180_open(struct inode *inodep, struct file *filep)
{
    pr_info( "BMP180 device opened\n");
    return 0;
}

static int bmp180_release(struct inode *inodep, struct file *filep)
{
    pr_info("BMP180 device closed\n");
    return 0;
}

static struct file_operations fops= {
    .open = bmp180_open,
    .owner = THIS_MODULE,
    .unlocked_ioctl = bmp180_ioctl,
    .release = bmp180_release,
};

static int bmp180_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct bmp180_data *data;
    int chip_id;
    int ret;

/*dynamic memory allocation*/
    data = devm_kzalloc(&client->dev, sizeof(struct bmp180_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    data->client = client;

    mutex_init(&data->lock);
    data->oversampling_setting = 3;

/*Link my data to bmp180_data by storing the pointer (*data) in i2c_client to store data (temperature, pressure values...) initially and access it again later*/
    i2c_set_clientdata(client, data);

    chip_id = i2c_smbus_read_byte_data(client, BMP180_CHIP_ID); // check chip ID
    if (chip_id != 0x55)
    {
        pr_err("Invalid chip ID: 0x%x\n", chip_id);
        return -ENODEV;
    }
    pr_info("Chip ID: 0x%x\n", chip_id);

    // Create a character device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ERR "Failed to register a major number\n");
        return major_number;
    }
    pr_info( "register successfully a major number: %d\n", major_number);

    // create a class
    bmp180_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(bmp180_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to register device class\n");
        return PTR_ERR(bmp180_class);
    }
    pr_info( "register a device class successfully\n");

    // create a device
    bmp180_device = device_create(bmp180_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(bmp180_device)) {
        class_destroy(bmp180_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to create the device\n");
        return PTR_ERR(bmp180_device);
    }
    pr_info("BMP180 driver installed\n");
    bmp180_client = client;

    // call read_E2PROM at first
    ret = bmp180_read_E2PROM(client);
    if (ret < 0)
    {
        device_destroy(bmp180_class, MKDEV(major_number, 0));
        class_unregister(bmp180_class);
        class_destroy(bmp180_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    return 0;
}

static int bmp180_remove(struct i2c_client *client)
{
    device_destroy(bmp180_class, MKDEV(major_number, 0));
    class_unregister(bmp180_class);
    class_destroy(bmp180_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info( "BMP180 driver removed\n");
    return 0;
}



static const struct of_device_id bmp180_of_match[] = {
    { .compatible = "bosch,bmp180" },
    { },
};
MODULE_DEVICE_TABLE(of, bmp180_of_match);


static struct i2c_driver bmp180_driver = {  
    .driver = {
        .name = DRIVER_NAME,
        .owner  = THIS_MODULE,
        .of_match_table =bmp180_of_match,
    },
    .probe = bmp180_probe,
    .remove = bmp180_remove,
};

static int __init bmp180_init(void)
{
    pr_info( "Initializing BMP180 driver\n");
    return i2c_add_driver(&bmp180_driver);
}

static void __exit mpu180_exit(void)
{
    printk( "exiting BMP180 driver\n");
    i2c_del_driver(&bmp180_driver);
}

module_init(bmp180_init);
module_exit(bmp180_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HoaiAn,PhucNguyen,SyNguyen,NamTin");
MODULE_DESCRIPTION("Writing a driver for the BMP180 sensor module");
