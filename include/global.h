#ifndef __GLOBAL_H
#define __GLOBAL_H

typedef struct {
    unsigned int baudrate;      //波特率
    unsigned int port;          //端口号 
    unsigned char dbit;         //数据位 
    char parity;                //奇偶校验
    unsigned char sbit;         //停止位
    char dev[30];               //dev下串口名字
    char mode[10];              //网络传输模式TCP UDP
    //char name[30];              //串口名字
}uart_cfg_t;
int serial_parameter_init(char * filename);
#define COM_PORT_BASE     1101          //端口号不能发生冲突,
#define SERIAL_NUMBER 4                 //串口数量
#define SERIAL_READ_BUF_LENGHT  256     //串口数据缓存长度

extern uart_cfg_t com[SERIAL_NUMBER];
#endif