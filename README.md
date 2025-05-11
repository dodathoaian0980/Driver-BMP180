# Driver-BMP180
Do Dat Hoai An 22146256

Nguyen Sy Nguyen 22146362

Tran Tho Phuc Nguyen 22146363

Nguyen Nam Tin 22146419 

## Chức Năng
Driver này cung cấp các chức năng sau:
* Đọc nhiệt độ từ cảm biến.
* Đọc áp suất từ cảm biến.
* Thiết lập chế độ oversampling (OSS) cho việc đo áp suất để cải thiện độ chính xác (từ 0 đến 3).

## Thông tin thiết bị
- Supply voltage: 1.8v - 3.6v
  
- BMP180 I2C address `0x77`

- Range: 30,000Pa..110,000Pa at 0°C..+65°C

- Typ. resolution: 1Pa / 0.1°C

- Typ. Absolute accuracy: ±100Pa / ±1.0°C

- Typ. relative accuracy: ±12Pa / ± 1.0 m

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

2. **Biên dịch và cài đặt module**
```js
make
sudo insmod driver_bmp180.ko
```
3. **Kiểm tra log kernel**
```js
dmesg | tail
```
4. **Chạy chương trình mẫu**
```js
gcc test.c -o run
sudo ./run
```
5. **Gở cài đặt module và dọn dẹp file**
```js
sudo rmmod driver_bmp180
make clean
```


- Không vượt quá điện áp cung cấp cho cảm biến.

# Tài liệu tham khảo
[Datasheet BMP180](https://cdn-shop.adafruit.com/datasheets/BST-BMP180-DS000-09.pdf)
