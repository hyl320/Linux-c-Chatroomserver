
#include"chatServer.h"

int main(int argc, char const *argv[])
{
    if(argc < 3)
    {
        cout<<"请输入ip地址和端口号"<<endl;
        return 0;
    }
    //传参正确 构造一个服务器
    chatServer server(argv[1],atoi(argv[2]));

    //启动服务器


    server.run();

    return 0;
}
