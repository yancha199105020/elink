#define _GNU_SOURCE     //在源文件开头定义_GNU_SOURCE宏
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
/***********************
 * 1.只有网络连接时端口时，才能创建串口读写，
 * 2.串口读数据时间太长了
 * 
 * 
 **************/
#define COM_PORT_BASE     1101    //端口号不能发生冲突,
#define SERIAL_NUMBER 4                 //串口数量
#define SERIAL_READ_BUF_LENGHT  256        //串口数据缓存长度
FILE *fp;

typedef struct {
    unsigned int baudrate;      /* 波特率 */
    unsigned char dbit;         /* 数据位 */
    char parity;                /* 奇偶校验 */
    unsigned char sbit;         /* 停止位 */
    unsigned char dev_name[30]; /* /dev下串口名字*/
}uart_cfg_t;

int fd[SERIAL_NUMBER]={0},connfd[SERIAL_NUMBER]={0};
int sockfd[SERIAL_NUMBER]={0};//network file descriptors
uart_cfg_t com[SERIAL_NUMBER]={0};//分别对应串口1，2，3，4
struct sockaddr_in client_addr[SERIAL_NUMBER] = {0};
pthread_t s_pid[SERIAL_NUMBER],n_pid[SERIAL_NUMBER];
char ip_str[20] = {0};

struct sockaddr_in cliAddr;
socklen_t cliLen = sizeof(cliAddr);

static int serial_read_flag;      //串口终端对应的文件描述符

void com_init(void){
    strcpy(com[0].dev_name,"/dev/ttyAMA0");
    com[0].baudrate=460800;
    strcpy(com[1].dev_name,"/dev/ttyAMA1");
    com[1].baudrate=460800;
    strcpy(com[2].dev_name,"/dev/ttyAMA2");
    com[2].baudrate=115200;
    strcpy(com[3].dev_name,"/dev/ttyAMA3");
    com[3].baudrate=115200;
}

/*进程退出处理函数*/
void exit_process(void){
    int i=0;
    for(i=0;i<SERIAL_NUMBER;i++){
        if(fd[i]>0){
            close(fd[i]);
        }
        if(sockfd[i]>0){
            close(sockfd[i]);
        }
        if(connfd[i]>0){
            close(connfd[i]);
        }
    }
    printf("exit success\n");
}
void parameter_init(void)
{
    int i=0;
    for(i=0;i<SERIAL_NUMBER;i++){
        fd[i]=-1;
        connfd[i]=-1;
        sockfd[i]=-1;
    }
}
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
    fd = open(serial->dev_name, O_RDWR | O_NOCTTY);
    if (fd<0) {
        fprintf(stderr, "open error: %s: %s\n", serial->dev_name, strerror(errno));
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

void *test1(void *arg){
    while(1){
        printf("1\n");
        sleep(1);
    }
}

void *test2(void *arg){
    while(1){
        printf("2\n");
        sleep(2);
    }
}
void *serial_read(void *arg)//arg传递端口号
{
    int ret=0,len=0;
    int i=0;
    unsigned char NO=0;
    unsigned char serial_read_buf[SERIAL_READ_BUF_LENGHT]={0};
    
    NO=*((char*)arg);
    printf("serial number=%d\n",NO);
    while(1)
    {
        ret = read(fd[NO], serial_read_buf, SERIAL_READ_BUF_LENGHT);     //一次最多读8个字节数据
        len=ret;
        printf("serial read is:");
        for(i=0;i<strlen(serial_read_buf);i++){
            printf("%02x ",serial_read_buf[i]);
        }
        if(NO<3){
            ret = send(connfd[NO], serial_read_buf, len, 0);
        }
        else{
            ret = sendto(sockfd[NO], serial_read_buf, len, 0, (struct sockaddr *)&cliAddr, cliLen);
        if (ret== -1)
        {
            perror("sendto");
            break;
        }
        }
        memset(serial_read_buf, 0x0, sizeof(serial_read_buf));
    }
}

void *network_pthread(void *arg){
    unsigned char NO=0;
    int ret=-1,len=0,i=0;
    static unsigned char flag=0;
    int addrlen = sizeof(client_addr[0]);
    unsigned char recvbuf[SERIAL_READ_BUF_LENGHT];
    unsigned char serial_read_buf[SERIAL_READ_BUF_LENGHT]={0};
    struct timeval old_tv,new_tv,tv;
    NO=*(unsigned char *)arg;
    while(1){
        if(NO==3){
            len = recvfrom(sockfd[NO], recvbuf, SERIAL_READ_BUF_LENGHT, 0, (struct sockaddr *)&cliAddr, &cliLen);
            
            write(fd[NO], recvbuf, len); 	//
            len = read(fd[NO], serial_read_buf, SERIAL_READ_BUF_LENGHT);     //从串口中读数据
            ret = sendto(sockfd[NO], serial_read_buf, len, 0, (struct sockaddr *)&cliAddr, cliLen);//UDP
            if (ret== -1)
            {
                perror("sendto");
                break;
            }
            continue;
        }
        connfd[NO] = accept(sockfd[NO], (struct sockaddr *)&client_addr[NO], &addrlen);
        if ( connfd[NO]<0) {
            perror("accept error");
            close(connfd[NO]);
            exit(EXIT_FAILURE);
        }

        printf("client connect com%d.\n",(NO+1));
        inet_ntop(AF_INET, &client_addr[NO].sin_addr.s_addr, ip_str, sizeof(ip_str));
        // printf("client ip: %s\n", ip_str);
        // printf("client port: %d\n", client_addr[NO].sin_port);
    /* 解析出参数 */
    
    /* 接收客户端发送过来的数据 */
        for ( ; ; ) {
        // 接收缓冲区清零
            memset(recvbuf, 0x0, SERIAL_READ_BUF_LENGHT);
            {
            
        // 读数据
            len = recv(connfd[NO], recvbuf, sizeof(recvbuf), 0);//从网口中读取数据
            gettimeofday(&old_tv, NULL);
            printf("com%d network recv\n",(NO+1));
            
            if(0 >= len) {
                perror("recv error");
                close(connfd[NO]);
                break;
            }
                    // 如果读取到"exit"则关闭套接字退出程序
            if (0 == strncmp("exit", recvbuf, 4)) {
                printf("server exit...\n");
                exit(EXIT_SUCCESS);
                return NULL;
            }
            write(fd[NO], recvbuf, len); 	//向串口写数据
            //printf("serial write...\n");
        // 将读取到的数据以字符串形式打印出来
            memset(serial_read_buf, 0x0, SERIAL_READ_BUF_LENGHT);
            gettimeofday(&old_tv, NULL);
            printf("com%d: %d\n",(NO+1),old_tv.tv_sec);
            printf("com%d: %d\n",(NO+1),old_tv.tv_usec);
            len = read(fd[NO], serial_read_buf, SERIAL_READ_BUF_LENGHT);     //从串口中读数据
            gettimeofday(&old_tv, NULL);
            printf("com%d serial read\n",(NO+1));
            printf("com%d after: %d\n",(NO+1),old_tv.tv_sec);
            printf("com%d after: %d\n",(NO+1),old_tv.tv_usec);
            ret = send(connfd[NO], serial_read_buf, len, 0);//TCP
            memset(serial_read_buf, 0x0, SERIAL_READ_BUF_LENGHT);
         
            }
        }
    }
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
    
    // int ret;
    // int n;

    // if(SIGRTMIN != sig)
    //     return;

    // /* 判断串口是否有数据可读 */
    // if (POLL_IN == info->si_code) {
    //     //ret = read(fd, serial_read_buf, 200);     //一次最多读8个字节数据
    //     serial_read_flag=1;
    //     printf("[ ");
    //     for (n = 0; n < ret; n++)
    //         printf("0x%hhx ", serial_read_buf[n]);
    //     printf("]\n");
    // }
}

/**
 ** 异步I/O初始化函数
 **/
static void async_io_init(void)
{
    // struct sigaction sigatn;
    // int flag;

    // /* 使能异步I/O */
    // flag = fcntl(fd, F_GETFL);  //使能串口的异步I/O功能
    // flag |= O_ASYNC;
    // fcntl(fd, F_SETFL, flag);

    // /* 设置异步I/O的所有者 */
    // fcntl(fd, F_SETOWN, getpid());

    // /* 指定实时信号SIGRTMIN作为异步I/O通知信号 */
    // fcntl(fd, F_SETSIG, SIGRTMIN);

    // /* 为实时信号SIGRTMIN注册信号处理函数 */
    // sigatn.sa_sigaction = io_handler;   //当串口有数据可读时，会跳转到io_handler函数
    // sigatn.sa_flags = SA_SIGINFO;
    // sigemptyset(&sigatn.sa_mask);
    // sigaction(SIGRTMIN, &sigatn, NULL);
}
int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr[SERIAL_NUMBER] = {0};
    unsigned char serial_address[SERIAL_NUMBER]={0};
    int ret=-1,i=0;//fd数组分别对应串口1，2，3，4的文件描述符号
    
    printf("1\n");
    atexit(exit_process);//注册进程结束处理函数
    com_init();
    parameter_init();

    //fp=fopen("debug.txt","W+");//打开调试文件
    /* 串口初始化 */
    for(i=0;i<SERIAL_NUMBER;i++){
        fd[i] = uart_init(&com[i]);
        if(fd[i]<0){
            exit(EXIT_FAILURE);
        }
    }
    printf("serial init success\n");
    /* 将套接字与指定端口号进行绑定 */

    /*端口123初始化为tcp，后续优化*/
    for(i=0;i<(SERIAL_NUMBER-1);i++){
        sockfd[i] = socket(AF_INET, SOCK_STREAM, 0);//tcp
        if ( sockfd[i]<0) {
            perror("socket error");
            exit(EXIT_FAILURE);
        }
        server_addr[i].sin_family = AF_INET;
        server_addr[i].sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr[i].sin_port = htons(COM_PORT_BASE+i);

        ret = bind(sockfd[i], (struct sockaddr *)&server_addr[i], sizeof(struct sockaddr_in));//绑定端口
        if ( ret<0) {
            perror("bind error");
            exit(EXIT_FAILURE);
        }

        ret = listen(sockfd[i], 50);//监听
        if (ret<0) {
            perror("listen error");
            exit(EXIT_FAILURE);
        }
        serial_address[i]=i;
        ret= pthread_create(&n_pid[i], NULL, network_pthread, (void*)&serial_address[i]);
        if(ret<0){
            perror("network pthread create error");
            exit(EXIT_FAILURE);
        }
        // ret= pthread_create(&s_pid[i], NULL, serial_read, (void*)&serial_address[i]);
        // if(ret<0){
        //     perror("serial pthread create error");
        //     exit(EXIT_FAILURE);
        // }
    }

    // ret= pthread_create(&s_pid[0], NULL, test1, NULL);
    // ret= pthread_create(&s_pid[0], NULL, test2, NULL);
    /*upd com4*/
    {
    i=3;
    sockfd[i] = socket(AF_INET, SOCK_DGRAM, 0);//udp
        if ( sockfd[i]<0) {
            perror("socket error");
            exit(EXIT_FAILURE);
        }
    server_addr[i].sin_family = AF_INET;
    server_addr[i].sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr[i].sin_port = htons(COM_PORT_BASE+i);
    ret = bind(sockfd[i], (struct sockaddr *)&server_addr[i], sizeof(struct sockaddr_in));//绑定端口
    if ( ret<0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }
    serial_address[i]=i;
    ret= pthread_create(&n_pid[i], NULL, network_pthread, (void*)&serial_address[i]);
    if(ret<0){
        perror("network pthread create error");
        exit(EXIT_FAILURE);
    }
    // ret= pthread_create(&s_pid[i], NULL, serial_read, (void*)&serial_address[i]);
    // if(ret<0){
    //     perror("serial pthread create error");
    //     exit(EXIT_FAILURE);
    // }

    }




    printf("listen...\n");
    /* 阻塞等待客户端连接 */
    
    while(1){
        ;
    }
    exit(EXIT_SUCCESS);
}
