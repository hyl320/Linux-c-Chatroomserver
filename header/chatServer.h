#ifndef CHATSERVER_H
#define CHATSERVER_H

#include<vector> //用于存储多个线程
#include<thread> //线程创建于引入

#include<queue> //用于存储多个任务的队列

#include<condition_variable> //条件变量有文件
#include<mutex> //引入互斥锁
#include<functional>
#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
using namespace std;
#include<string>
#include<unistd.h>
#include<sys/wait.h> //wait进程资源回收函数所在的头文件
#include<sys/sem.h>
#include<sys/stat.h>
#include<signal.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<sys/shm.h>
#include<pthread.h>
#include<semaphore.h>
#include<thread>
#include <arpa/inet.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<poll.h>
#include<sys/epoll.h>
#include<sys/un.h>
#include<sys/sendfile.h>
#include<sqlite3.h>
#include<time.h>

#define N 128
#define LOGIN 1
#define CHAT 2
#define QUIT 3

//自定义服务器类
class chatServer
{
public:
   //定义消息结构体
   struct MSG
   {
        int type;         //消息类型
        char name[20];    //客户端名称
        char text[N];     //消息内容

        //序列化函数 将消息结构体转换成二进制数据 便于传输
        string serialize() const
        {
            string data; //临时要存储的字符串
            data.append(reinterpret_cast<const char*>(&type),sizeof(type)); //将消息类型放入字符串
            data.append(name,sizeof(name));
            data.append(text,sizeof(text));

            return data;
        }

        //反序列化函数 二进制数据转化为结构体
        void deserialize(const string &data)
        {
            size_t offset = 0; //截止字符串时的偏移量

            memcpy(&type,data.c_str()+offset ,sizeof(type)); //将字符串的消息类型解析出来
            offset += sizeof(type);

            memcpy(&name,data.c_str()+offset,sizeof(name)); //将字符串中客户端名字解析出来
            offset += sizeof(name);

            memcpy(&text,data.c_str()+offset,sizeof(text));

        }

   };
   //客户端结构体类型
   struct Client
   {
        int fd;
        struct sockaddr_in cin; //客户端地址信息结构体
   };

private:
   int sfd; //服务器套接字
   vector<Client> clients;     //在线的客户端列表
   mutex client_mutex;         //保护clients列表

   //线程池相关变量
   //线程池相关的私有成员
   vector<thread> workers;              //存储工作的线程容器
   queue<function<void()>> tasks;        //存储任务的队列
   mutex task_mutex;                    //互斥锁
   condition_variable task_cv;          //用于通知线程有新任务的条件变量 
   bool stop;                           //线程池的标志 判断线程池是否停止

   void errlog(const char* msg);            //错误信息日志函数
   void startThreadPool(size_t numThreads);  //启动线程池函数
   void addTask(function<void()> task);                           //将任务添加的线程池中


public:


    chatServer(const char* ip , int port , size_t threadPOOlsize = 4); //构造函数的说明
    ~chatServer();
    void run();        //运行服务器函数
    void handlerClient(int client_fd, struct sockaddr_in cin); //处理客户端连接的函数
    void broadcast(const MSG& msg,int exclude_fd = -1); //广播消息函数


};
#endif 