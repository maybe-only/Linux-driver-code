#include<sys/time.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>

int main()
{
	
    char buf[10] = "";
    fd_set rdfds;
    struct timeval tv;
    int ret;
	
    FD_ZERO(&rdfds);
    FD_SET(0,&rdfds);   //文件描述符0表示stdin键盘输入
	
    tv.tv_sec = 3;
    tv.tv_usec = 500;
	
    ret = select(1,&rdfds,NULL,NULL,&tv);	   //第一个参数是监控句柄号+1
    if(ret < 0)
        printf("selcet error\r\n");
    else if(ret == 0)
        printf("timeout \r\n");
    else
        printf("ret = %d \r\n",ret);
 
    if(FD_ISSET(0,&rdfds)){  			//监控输入的确是已经发生了改变
        printf(" reading");
        read(0,buf,9);                 	//从键盘读取输入
    }
	
    write(1,buf,strlen(buf));  			//在终端中回显
	
    printf(" %d \r\n",strlen(buf));
	
    return 0;
}
