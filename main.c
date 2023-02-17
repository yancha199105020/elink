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
#include "global.h"
#include "uart.h"
int fd[SERIAL_NUMBER]={0},connfd[SERIAL_NUMBER]={0};
int sockfd[SERIAL_NUMBER]={0};//network file descriptors
uart_cfg_t com[SERIAL_NUMBER]={0};//分别对应串口1，2，3，4
struct sockaddr_in client_addr[SERIAL_NUMBER] = {0};
pthread_t s_pid[SERIAL_NUMBER],n_pid[SERIAL_NUMBER];
struct sockaddr_in cliAddr;
socklen_t cliLen = sizeof(cliAddr);

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
void parameter_init(void){
    int i=0;
    for(i=0;i<SERIAL_NUMBER;i++){
        fd[i]=-1;
        connfd[i]=-1;
        sockfd[i]=-1;
    }
}


void *network_tcp_pthread(void *arg){
    unsigned char NO=0;
    int ret=-1,len=0,i=0;
    static unsigned char flag=0;
    int addrlen = sizeof(client_addr[0]);
    unsigned char recvbuf[SERIAL_READ_BUF_LENGHT];
    unsigned char serial_read_buf[SERIAL_READ_BUF_LENGHT]={0};
    struct timeval old_tv,new_tv,tv;
    NO=*(unsigned char *)arg;
    while(1){
        connfd[NO] = accept(sockfd[NO], (struct sockaddr *)&client_addr[NO], &addrlen);
        if ( connfd[NO]<0) {
            perror("accept error");
            close(connfd[NO]);
            exit(EXIT_FAILURE);
        }

        printf("client connect com%d.\n",(NO+1));
        //inet_ntop(AF_INET, &client_addr[NO].sin_addr.s_addr, ip_str, sizeof(ip_str));
    
    /* 接收客户端发送过来的数据 */
        for ( ; ; ) {
        // 接收缓冲区清零
            memset(recvbuf, 0x0, SERIAL_READ_BUF_LENGHT);            
        // 读数据
            len = recv(connfd[NO], recvbuf, sizeof(recvbuf), 0);//从网口中读取数据
            if(len<=0) {
                perror("recv error");
                close(connfd[NO]);
                break;
            }
            gettimeofday(&old_tv, NULL);
            printf("com%d network recv: %d\n",(NO+1),old_tv.tv_sec);
            printf("com%d: %d\n",(NO+1),old_tv.tv_usec);
            
            // 如果读取到"exit"则关闭套接字退出程序
            if (0 == strncmp("exit", recvbuf, 4)) {
                printf("server exit...\n");
                exit(EXIT_SUCCESS);
            }
            write(fd[NO], recvbuf, len); 	//向串口写数据
            //printf("serial write...\n");
        // 将读取到的数据以字符串形式打印出来
            memset(serial_read_buf, 0x0, SERIAL_READ_BUF_LENGHT);
        
            len = read(fd[NO], serial_read_buf, SERIAL_READ_BUF_LENGHT);     //从串口中读数据
            if(len<=0) {
                perror("read error");
                close(connfd[NO]);
                break;
            }
            gettimeofday(&old_tv, NULL);
            printf("com%d serial read\n",(NO+1));
            printf("com%d after: %d\n",(NO+1),old_tv.tv_sec);
            printf("com%d after: %d\n",(NO+1),old_tv.tv_usec);
            ret = send(connfd[NO], serial_read_buf, len, 0);//TCP
            if(len<=0) {
                perror("send error");
                close(connfd[NO]);
                break;
            }
            memset(serial_read_buf, 0x0, SERIAL_READ_BUF_LENGHT);
        }
    }
}

void *network_udp_pthread(void *arg){
    unsigned char NO=0;
    int ret=-1,len=0,i=0;
    static unsigned char flag=0;
    int addrlen = sizeof(client_addr[0]);
    unsigned char recvbuf[SERIAL_READ_BUF_LENGHT];
    unsigned char serial_read_buf[SERIAL_READ_BUF_LENGHT]={0};
    struct timeval old_tv,new_tv;
    NO=*(unsigned char *)arg;
    while(1){
        
            len = recvfrom(sockfd[NO], recvbuf, SERIAL_READ_BUF_LENGHT, 0, (struct sockaddr *)&cliAddr, &cliLen);
            gettimeofday(&old_tv, NULL);
            printf("com%d: %d\n",(NO+1),old_tv.tv_sec);
            printf("com%d: %d\n",(NO+1),old_tv.tv_usec);;
            printf("com%d udp recv\n",(NO+1));
            write(fd[NO], recvbuf, len); 	//
            len = read(fd[NO], serial_read_buf, SERIAL_READ_BUF_LENGHT);     //从串口中读数据
            gettimeofday(&old_tv, NULL);
            printf("com%d serial read\n",(NO+1));
            printf("com%d after: %d\n",(NO+1),old_tv.tv_sec);
            printf("com%d after: %d\n",(NO+1),old_tv.tv_usec);
            ret = sendto(sockfd[NO], serial_read_buf, len, 0, (struct sockaddr *)&cliAddr, cliLen);//UDP
            if (ret== -1)
            {
                perror("sendto");
                exit(EXIT_FAILURE);
            }
    }
}


int main(int argc, char *argv[])//启动是传入工程路径
{
    struct sockaddr_in server_addr[SERIAL_NUMBER] = {0};
    unsigned char serial_address[SERIAL_NUMBER]={0};
    int ret=-1,i=0;
    char home_path[200]={0};
    int ret=0;
    char *check=NULL;
    if(argc<2){//检查路径
        printf("home root not exist\n");
        exit(EXIT_FAILURE);
    }
    check=strcpy(home_path,argv[1]);
    if(NULL==check){
        printf("home root is empty\n");
        exit(EXIT_FAILURE);
    }
    printf("home path:%s\n",home_path);
    strcat(home_path,"/config.yaml");
    atexit(exit_process);//注册进程结束处理函数
    serial_parameter_init(home_path);
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

    for(i=0;i<(SERIAL_NUMBER);i++){
        if(0==strcmp(com->mode,"tcp")){//tcp  模式
            sockfd[i] = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd[i]<0){
                perror("socket error");
                exit(EXIT_FAILURE);
            }
            server_addr[i].sin_family = AF_INET;
            server_addr[i].sin_addr.s_addr = htonl(INADDR_ANY);
            server_addr[i].sin_port = htons(COM_PORT_BASE+i);

            ret = bind(sockfd[i], (struct sockaddr *)&server_addr[i], sizeof(struct sockaddr_in));//绑定端口
            if(ret<0){
                perror("bind error");
                exit(EXIT_FAILURE);
            }

            ret = listen(sockfd[i], 50);//监听
            if(ret<0) {
                perror("listen error");
                exit(EXIT_FAILURE);
            }
            serial_address[i]=i;
            ret= pthread_create(&n_pid[i], NULL, network_tcp_pthread, (void*)&serial_address[i]);
            if(ret<0){
                perror("network tcp pthread create error");
                exit(EXIT_FAILURE);
            }
        }
        else if(0==strcmp(com->mode,"udp")){
            sockfd[i] = socket(AF_INET, SOCK_DGRAM, 0);//udp
            if(sockfd[i]<0){
                perror("socket error");
                exit(EXIT_FAILURE);
            }
            server_addr[i].sin_family = AF_INET;
            server_addr[i].sin_addr.s_addr = htonl(INADDR_ANY);
            server_addr[i].sin_port = htons(COM_PORT_BASE+i);
            ret = bind(sockfd[i], (struct sockaddr *)&server_addr[i], sizeof(struct sockaddr_in));//绑定端口
            if(ret<0) {
                perror("bind error");
                exit(EXIT_FAILURE);
            }   
            serial_address[i]=i;
            ret= pthread_create(&n_pid[i], NULL, network_udp_pthread, (void*)&serial_address[i]);
            if(ret<0){
                perror("network udp pthread create error");
                exit(EXIT_FAILURE);
            }
        }
        else {
            printf("network tranmit mode error\n");
            exit(EXIT_FAILURE);
        }
    }

    printf("listen...\n");
    /* 阻塞等待客户端连接 */
    
    while(1){
        ;//心跳
    }
    exit(EXIT_SUCCESS);
}