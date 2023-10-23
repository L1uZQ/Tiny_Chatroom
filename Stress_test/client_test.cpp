#include<iostream>
#include<cstring>
#include<unistd.h>
#include<arpa/inet.h>
#include<jsoncpp/json/json.h>
#include"ProtocolHead/DataEncoder.h"
#include"ProtocolHead/HeadData.h"

int main()
{
    //创建套接字
    int fd = socket(AF_INET,SOCK_STREAM,0);
    if(fd==-1){
        perror("socket");
        exit(0);
    }

    //连接服务器
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9007);   // 大端端口
    inet_pton(AF_INET, "43.136.177.64", &addr.sin_addr.s_addr);
    
    int ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret == -1){
        perror("connect");
        exit(0);
    }

    DataEncoder encoder;
    string account = "102";
    string password = "254003";
    Json::Value loginMsg;
    loginMsg["account"] = account;
    loginMsg["password"]= password;
    Json::StreamWriterBuilder writer;
    std::string loginMsgstr = Json::writeString(writer, loginMsg);
    // string loginMsgstr = loginMsg.toStyledString();
    //报文头部
    string headstr = encoder.encode(LOGIN,stoi(account),TEXT,loginMsgstr.length());

    write(fd, headstr.c_str(), headstr.length());

    if(loginMsgstr.length()!=0)
    {
        write(fd, loginMsgstr.c_str(),loginMsgstr.length());
        std::cout<<"发送成功，数据总大小:"<<loginMsgstr.length()<<std::endl;
    }

    HeadData hd(fd);
    int datalength = hd.getDataLength(); 
    char buf[256];
    read(fd,buf, datalength);
    string recvMsg = string(buf,datalength);
    Json::Reader jsonReader;
    Json::Value msg;
    jsonReader.parse(recvMsg, msg);

    if(msg["status"]==LOGIN_SUCCESS){
        cout<<"登陆成功"<<endl;
        //主线程每隔10秒发一条消息，后台另外开一个线程用于收消息
        
    }
}



