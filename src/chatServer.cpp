#include "chatServer.h"

// 构造函数的定义：c初始化服务器、创建套接字、绑定地址信息结构体、启动监听
chatServer::chatServer(const char *ip, int port,size_t threadPOOlsize):stop(false)
{
    // 创建服务器套接字
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    // cout<<"sfd:"<<sfd;
    if (sfd < 0)
    {
        perror("socket error");
        return;
    }
    // 端口号快速重用
    int reuse = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt error");
        return;
    }

    // 填充服务器地址信息结构体
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, ip, &sin.sin_addr);

    if (bind(sfd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("bind error");
        close(sfd);
        sfd = -1;
        return;
    }
    // 启动监听
    if (listen(sfd, 10) < 0)
    {
        perror("listen error");
        close(sfd);
        sfd = -1;
        return;
    }

    // 启动线程池
    startThreadPool(threadPOOlsize);
}

chatServer::~chatServer()
{
    stop = true; // 设置停止标志 表示停止线程池
    task_cv.notify_all();
    for (auto &worker : workers)
    {
        worker.join(); // 等待回收所有线程
    }
    close(sfd); // 关闭服务器套接字
}

// 启动线程池
void chatServer::startThreadPool(size_t numThreads)
{
    // 循环创建numThreads个线程
    for (int i = 0; i < numThreads; i++)
    {
        // 创建一个线程 并且将线程放入到线程容器中
        workers.emplace_back([this]
                             {
            while(1)
            {
                function<void()> task;
                {
                    unique_lock<mutex> lock(task_mutex);
                    task_cv.wait(lock,[this]{
                        return stop || !tasks.empty();
                    });
                    if(stop && tasks.empty())
                    {
                        return;
                    }
                    task = move(tasks.front());
                    tasks.pop();
                }
                task();                //执行任务
            } });
    }
}
// 错误信息日志函数
void chatServer::errlog(const char *msg)
{
    cerr << __FILE__ << " " << __func__ << " " << __FILE__ << endl;
    perror(msg);
}

void chatServer::addTask(function<void()> task)
{
    {
        unique_lock<mutex> lock(task_mutex); // 保护条件变量
        tasks.push(task);
    }
    // 唤醒一个等待的线程开始工作
    task_cv.notify_one(); // 唤醒一个线程工作
}
void chatServer::run()       //运行服务器函数
{
    if (sfd < 0) {
        fprintf(stderr, "Error: Server socket invalid (fd=%d). Check initialization logs.\n", sfd);
        return;  // 或 throw exception，取决于您的设计
    }
    //循环等待客户端连接
    while(1)
    {
        struct sockaddr_in cin;
        socklen_t clen =  sizeof(cin);
        memset(&cin, 0, sizeof(cin)); 
        int client_fd = accept(sfd,(struct sockaddr*)&cin,&clen);
        if(client_fd <0)
        {
            fprintf(stderr, "Error number: %d\n", errno);
            errlog("accept error");
            continue;
        }
        //接收完之后将处理客户端信息的任务 添加到线程池
        addTask([this,client_fd,cin]{
            //函数体就是处理客户端的功能函数
            this->handlerClient(client_fd,cin);
        });
    }
}
//处理客户端
void chatServer::handlerClient(int client_fd, struct sockaddr_in cin)
{
    //定义一个消息类型的变量 用来接受客户端的消息
    MSG msg;
    char buffer[sizeof(MSG)]; //初始的消息形态

    //循环接收客户端的信息
    while(1)
    {
        int recv_len = recv(client_fd,buffer,sizeof(buffer),0);
        if(recv_len <= 0)
        {
            //表示客户端已经下线
            //向其他客户端 发送消息
            //遍历客户端容器
            auto it = clients.begin();         //定义客户端容器的迭代器
            while(it != clients.end())
            {
                if(it->fd == client_fd)         //在容器中找到当前套接字
                {
                    clients.erase(it);          //移除当前套接字
                    break;
                }
                ++it;                    
            }
            close(client_fd);
            break;
        }
        msg.deserialize(string(buffer,recv_len));
        //对消息类型进行判断
        switch (ntohl(msg.type))
        {
        case LOGIN:
            {
                //表示客户端发来的是登录消息
                //定义一个互斥锁保护客户端容器
                unique_lock<mutex> lock(client_mutex);
                Client new_client;
                //将新客户端的信息获取下来
                new_client.fd = client_fd;
                new_client.cin = cin;

                //将新客户端放入客户端容器中
                clients.push_back(new_client);
                //组装一条消息 广播给所有的客户端
                sprintf(msg.text,"---------------%s登录成功------------",msg.name);
                broadcast(msg);
                break;                //告诉所有用户 当前用户登录成功
            }
            
        case CHAT:
            {
                //表示客户端发来的是聊天消息
                unique_lock<mutex> lock(client_mutex);
                broadcast(msg,client_fd);      //除了自己发送给所有人

            }
            break;
        case QUIT: 
            {
                //表示客户端发来的是退出消息
                unique_lock<mutex> lock(client_mutex);
                auto it = clients.begin();         //定义迭代器，遍历客户端容器
                while(it != clients.end())
                {
                    if(it->fd == client_fd)
                    {

                        clients.erase(it);
                        break;
                        
                    }              
                    ++it;
                }
                sprintf(msg.text,"---------%s 退出聊天室------------",msg.name);
                //将广播发送给所有人
                broadcast(msg);
                close(client_fd); //关闭当前客户端套接字
                break;
            }
            
            
        default:
            cout<<"消息类型有误"<<endl;
            cout<<msg.type<<endl;
            cout<<msg.name<<endl;
            cout<<msg.text<<endl;
            break;
        }
    }
}

//定义广播函数

void chatServer::broadcast(const MSG& msg,int exclude_fd)
{
    //将要广播的消息进行序列化操作 转换成字符串
    string data =  msg.serialize();

    //将字符串转发给所有客户端
    for(const auto &client:clients)
    {
        if(client.fd != exclude_fd)
        {
            if(send(client.fd,data.c_str(),data.size(),0)<0)
            {
                perror("send error");
                return;
            }
        }
        
    }
}