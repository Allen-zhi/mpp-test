# mpp-test
## vo_show_nv12_test

### build
./build.sh
aarch64-linux-gnu-gcc -o vo_show_nv12_test vo_show_nv12_test.c -pthread libdrm.so.2.4.0 librockchip_mpp.so

### using
usage example: ./vo_test -d 1 -i test.yuv -w 1920 -h 1080
