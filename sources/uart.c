#define _GNU_SOURCE     //在源文件开头定义_GNU_SOURCE宏
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include "global.h"
#include "uart.h"
/**
 ** 串口初始化操作
 ** 参数serial表示串口终端的设备节点
 ** 返回文件描述符
 **/
static int uart_init(uart_cfg_t *serial)
{
    /* 打开串口终端 */
    int fd=-1;
    struct termios new_cfg = {0};   //将new_cfg对象清零
    speed_t speed;
    fd = open(serial->dev, O_RDWR | O_NOCTTY);
    if (fd<0) {
        fprintf(stderr, "open error: %s: %s\n", serial->dev, strerror(errno));
        return -1;
    }
    printf("serial open success\n");
   
    /* 设置为原始模式 */
    cfmakeraw(&new_cfg); //直接用串口收发数据，不把串口当作终端使用

    /* 使能接收 */
    new_cfg.c_cflag |= CREAD;

    /* 设置波特率 */
    switch (serial->baudrate) {
    case 1200: speed = B1200;
        break;
    case 1800: speed = B1800;
        break;
    case 2400: speed = B2400;
        break;
    case 4800: speed = B4800;
        break;
    case 9600: speed = B9600;
        break;
    case 19200: speed = B19200;
        break;
    case 38400: speed = B38400;
        break;
    case 57600: speed = B57600;
        break;
    case 115200: speed = B115200;
        break;
    case 230400: speed = B230400;
        break;
    case 460800: speed = B460800;
        break;
    case 500000: speed = B500000;
        break;
    default:    //默认配置为115200
        speed = B115200;
        printf("default baud rate: 115200\n");
        break;
    }

    if (0 > cfsetspeed(&new_cfg, speed)) {
        fprintf(stderr, "cfsetspeed error: %s\n", strerror(errno));
        close(fd);
        return -1;
    }  
    /* 设置数据位大小 */
    new_cfg.c_cflag &= ~CSIZE;  //将数据位相关的比特位清零
    switch (serial->dbit) {
    case 5:
        new_cfg.c_cflag |= CS5;
        break;
    case 6:
        new_cfg.c_cflag |= CS6;
        break;
    case 7:
        new_cfg.c_cflag |= CS7;
        break;
    case 8:
        new_cfg.c_cflag |= CS8;
        break;
    default:    //默认数据位大小为8
        new_cfg.c_cflag |= CS8;
        printf("default data bit size: 8\n");
        break;
    }

    /* 设置奇偶校验 */
    switch (serial->parity) {
    case 'N':       //无校验
        new_cfg.c_cflag &= ~PARENB;
        new_cfg.c_iflag &= ~INPCK;
        break;
    case 'O':       //奇校验
        new_cfg.c_cflag |=(PARODD | PARENB);
        new_cfg.c_iflag |= INPCK;
        break;
    case 'E':       //偶校验
        new_cfg.c_cflag |= PARENB;
        new_cfg.c_cflag &= ~PARODD; /* 清除PARODD标志，配置为偶校验 */
        new_cfg.c_iflag |= INPCK;
        break;
    default:    //默认配置为无校验
        new_cfg.c_cflag &= ~PARENB;
        new_cfg.c_iflag &= ~INPCK;
        printf("default parity: N\n");
        break;
    }

    /* 设置停止位 */
    switch (serial->sbit) {
    case 1:     //1个停止位
        new_cfg.c_cflag &= ~CSTOPB;
        break;
    case 2:     //2个停止位
        new_cfg.c_cflag |= CSTOPB;
        break;
    default:    //默认配置为1个停止位
        new_cfg.c_cflag &= ~CSTOPB;
        printf("default stop bit size: 1\n");
        break;
    }

    /* 将MIN和TIME设置为0 */
    new_cfg.c_cc[VTIME] = 0;
    new_cfg.c_cc[VMIN] = 1;

    /* 清空缓冲区 */
    if (0 > tcflush(fd, TCIOFLUSH)) {
        fprintf(stderr, "tcflush error: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    /* 写入配置、使配置生效 */
    if (0 > tcsetattr(fd, TCSANOW, &new_cfg)) {//配置立即生效
        fprintf(stderr, "tcsetattr error: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    /* 配置OK 退出 */
    return fd;//
}


static void show_help(const char *app)
{
    printf("Usage: %s [选项]\n"
        "\n必选选项:\n"
        "  --dev=DEVICE     指定串口终端设备名称, 譬如--dev=/dev/ttymxc2\n"
        "  --type=TYPE      指定操作类型, 读串口还是写串口, 譬如--type=read(read表示读、write表示写、其它值无效)\n"
        "\n可选选项:\n"
        "  --brate=SPEED    指定串口波特率, 譬如--brate=115200\n"
        "  --dbit=SIZE      指定串口数据位个数, 譬如--dbit=8(可取值为: 5/6/7/8)\n"
        "  --parity=PARITY  指定串口奇偶校验方式, 譬如--parity=N(N表示无校验、O表示奇校验、E表示偶校验)\n"
        "  --sbit=SIZE      指定串口停止位个数, 譬如--sbit=1(可取值为: 1/2)\n"
        "  --help           查看本程序使用帮助信息\n\n", app);
}
/**
 ** 信号处理函数，当串口有数据可读时，会跳转到该函数执行
 **/
static void io_handler(int sig, siginfo_t *info, void *context)
{
    
    int ret;
    int n;

    if(SIGRTMIN != sig)
        return;

    /* 判断串口是否有数据可读 */
    if (POLL_IN == info->si_code) {
        //ret = read(fd, serial_read_buf, 200);     //一次最多读8个字节数据
        printf("[ ");
        for (n = 0; n < ret; n++)
            //printf("0x%hhx ", serial_read_buf[n]);
        printf("]\n");
    }
}

/**
 ** 异步I/O初始化函数
 **/
static void async_io_init(int fd)
{
    struct sigaction sigatn;
    int flag;

    /* 使能异步I/O */
    flag = fcntl(fd, F_GETFL);  //使能串口的异步I/O功能
    flag |= O_ASYNC;
    fcntl(fd, F_SETFL, flag);

    /* 设置异步I/O的所有者 */
    fcntl(fd, F_SETOWN, getpid());

    /* 指定实时信号SIGRTMIN作为异步I/O通知信号 */
    fcntl(fd, F_SETSIG, SIGRTMIN);

    /* 为实时信号SIGRTMIN注册信号处理函数 */
    sigatn.sa_sigaction = io_handler;   //当串口有数据可读时，会跳转到io_handler函数
    sigatn.sa_flags = SA_SIGINFO;
    sigemptyset(&sigatn.sa_mask);
    sigaction(SIGRTMIN, &sigatn, NULL);
}
