# Driver-BMP180
Do Dat Hoai An 22146256

Nguyen Sy Nguyen 22146362

Tran Tho Phuc Nguyen 22146363

Nguyen Nam Tin 22146419

# Module cảm biến áp suất BMP180
```js
Nguyễn Trường Giang - MSSV: 22146298
Huỳnh Tấn Hà Duy - MSSV: 22146284
Nguyễn Công Danh - MSSV: 22146280
Nguyễn Ngọc Độ - MSSV: 22146294
```
## Giới thiệu
Đây là source code driver cho module cảm biến áp suất và nhiệt độ BMP180, giao tiếp thông qua giao thức I2C. Driver cho phép:

- Đọc và tính toán dữ liệu nhiệt độ, áp suất, và độ cao so với mực nước biển.

- Giao tiếp với lớp user space và cho phép người dùng chọn chế độ đo và tần số lấy mẫu.
## Thông tin thiết bị
- Supply voltage: 1.8v - 3.6v
  
- BMP180 I2C address `0x77`

- Range: 30,000Pa..110,000Pa at 0°C..+65°C

- Typ. resolution: 1Pa / 0.1°C

- Typ. Absolute accuracy: ±100Pa / ±1.0°C

- Typ. relative accuracy: ±12Pa / ± 1.0 m
## Tính năng driver
- Tạo character device để giao tiếp với cảm biến BMP180.

- Hỗ trợ đọc dữ liệu với các mức oversampling_setting (OSS) khác nhau.

- Đọc ID chip, đọc và tính toán nhiệt độ ( đơn vị °C ), đọc và tính toán áp suất khí quyển ( đơn vị Pa ), tính toán độ cao so với mặt nước biển ( đơn vị m ).

- Giao tiếp với user space để:

  - Lựa chọn tần số lấy mẫu.

  - Chọn chế độ đo: nhiệt độ / áp suất / độ cao.

  - Hiển thị kết quả ra màn hình.
    
## Hướng dẫn sử dụng
1. **Kiểm tra cảm biến đã được kết nối và nhận diện trên hệ thống hay chưa**
```js
sudo i2cdetect -y 1
```
- Nếu xuất hiện địa chỉ 0x77, kết nối thành công

- Nếu xuất hiện địa chỉ UU tại vị trí 0x77, ta tiến hành gở lỗi
 ```js
echo 1-0077 | sudo tee /sys/bus/i2c/devices/1-0077/driver/unbind
```
2. **Thêm cấu hình module BMP180 vào Device Tree**
```js
dtc -I dtb -O dts -o bcm2712-rpi-5-b.dts -o bcm2712-rpi-5-b.dtb
dtc -I dts -O dtb -o bcm2712-rpi-5-b.dts bcm2712-rpi-5-b.dtb
```
3. **Cấu hình bmp180 trong Device Tree**
```js
bmp180@77 {
              compatible = "bosch,bmp180";
               reg = <0x77>;
};
```
4. **Biên dịch và cài đặt module**
```js
make
sudo insmod driver_bmp180.ko
```
5. **Kiểm tra log kernel**
```js
dmesg | tail
```
6. **Chạy chương trình mẫu**
```js
gcc test.c -o run
sudo ./run
```
7. **Gở cài đặt module và dọn dẹp file**
```js
sudo rmmod driver_bmp180
make clean
```

## Khuyến nghị
- Driver sử dụng thông tin hiệu chỉnh (calibration data) từ cảm biến theo hướng dẫn trong **datasheet BMP180**.

- Việc đo áp suất khí quyển phụ thuộc vào nhiệt độ môi trường xung quanh. Tránh đặt cảm biến BMP180 trước nguồn nhiệt.

- Không để BMP180 tiếp xúc với luồng gió từ quạt, vì điều này có thể dẫn đến kết quả đo không ổn định.

- Cảm biến nhạy với độ ẩm và không được khuyến nghị tiếp xúc trực tiếp với nước.

- BMP180 nhạy với ánh sáng. Không để cảm biến tiếp xúc với ánh sáng trực tiếp.

- Không vượt quá điện áp cung cấp cho cảm biến.

# Tài liệu tham khảo
[Datasheet BMP180](https://cdn-shop.adafruit.com/datasheets/BST-BMP180-DS000-09.pdf)
