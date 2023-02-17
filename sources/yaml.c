#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "global.h"
#include "yaml.h"
#include "libfyaml.h"
#include "sys_log.h"


int serial_parameter_init(char * filename){
    struct fy_document* fyd;
    struct fy_node *fyn, *fyn_root, *fyn_serial, *fyn_para;
    const char *buff=NULL;
    char path[100]={0};
    size_t len=0;
    int count=0,i=0;
    fyd = fy_document_build_from_file(NULL,filename); //open  yaml file
    if (!fyd){
        sys_log_erro("failed to open serial config file\n");
        return -1;
    }
    else
    {
        sys_log_debug("success to open serial config file,serial,config file=%s\n",filename);
    }
    fyn_root = fy_document_root(fyd);//文档根目录
    if(NULL==fyn_root){
        return -1;
    }
    fyn_serial = fy_node_by_path(fyn_root, "/serial", FY_NT, FYNWF_DONT_FOLLOW);//找到串口
    if(NULL==fyn_serial){
        return -1;            
    }
    count = fy_node_sequence_item_count(fyn_serial);
    if(count<0){
        return -1;
    }
    for(int i=0;i<count;i++){
        snprintf(path, 100, "/[%d]/dev", i);
        fyn_para = fy_node_by_path(fyn_serial, path, FY_NT, FYNWF_DONT_FOLLOW);
        buff = fy_node_get_scalar(fyn_para, &len);
        strcpy(com[i].dev,buff);

        snprintf(path, 100, "/[%d]/mode", i);
        fyn_para = fy_node_by_path(fyn_serial, path, FY_NT, FYNWF_DONT_FOLLOW);
        buff = fy_node_get_scalar(fyn_para, &len);
        strcpy(com[i].mode,buff);

        snprintf(path, 100, "/[%d]/port", i);
        fyn_para = fy_node_by_path(fyn_serial, path, FY_NT, FYNWF_DONT_FOLLOW);
        buff = fy_node_get_scalar(fyn_para, &len);
        com[i].port=atoi(buff);

        snprintf(path, 100, "/[%d]/baudrate", i);
        fyn_para = fy_node_by_path(fyn_serial, path, FY_NT, FYNWF_DONT_FOLLOW);
        buff = fy_node_get_scalar(fyn_para, &len);
        com[i].baudrate=atoi(buff);
        printf("com%d:\n",(i+1));
        printf("dev=%s\n",com[i].dev);
        printf("mode=%s\n",com[i].mode);
        printf("port=%d\n",com[i].port);
        printf("baudrate=%d\n",com[i].baudrate);
        printf("\n");
    }
}
