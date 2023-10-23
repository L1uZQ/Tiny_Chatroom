#include<iostream>
#include<cstring>
#include<unistd.h>
#include<arpa/inet.h>
#include<thread>
#include <chrono>
#include<jsoncpp/json/json.h>
#include"../ProtocolHead/DataEncoder.h"
#include"../ProtocolHead/HeadData.h"



//实现账号的快速注册，用于测试注册并发功能以及大批量生成账号
void registerAccount(int accountNum) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(0);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9007);
    inet_pton(AF_INET, "43.136.177.64", &addr.sin_addr.s_addr);

    int ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) {
        perror("connect");
        exit(0);
    }

    DataEncoder encoder;
    std::string account = std::to_string(accountNum);
    std::string username = account;
    std::string password = "254003"; // 你可以设置不同的密码

    Json::Value regisMsg;
    regisMsg["account"] = account;
    regisMsg["username"] = username;
    regisMsg["password"] = password;
    Json::StreamWriterBuilder writer;
    std::string regisMsgstr = Json::writeString(writer, regisMsg);

    std::string headstr = encoder.encode(REGISTER, std::stoi(account), TEXT, regisMsgstr.length());

    write(fd, headstr.c_str(), headstr.length());

    if (regisMsgstr.length() != 0) {
        write(fd, regisMsgstr.c_str(), regisMsgstr.length());
        std::cout << "发送成功，数据总大小: " << regisMsgstr.length() << std::endl;
    }

    HeadData hd(fd);
    int datalength = hd.getDataLength();
    char buf[256];
    read(fd, buf, datalength);
    std::string recvMsg = std::string(buf, datalength);
    Json::Reader jsonReader;
    Json::Value msg;
    jsonReader.parse(recvMsg, msg);

    if (msg["status"] == REGISTER_SUCCESS) {
        std::cout << account << "注册成功" << std::endl;
    }else if(msg["status"] == REGISTER_FAIL){
        cout<< account <<"注册失败"<<endl;
    }
    close(fd);
}

int main() {
    // 创建50个线程，每个线程注册一个账号
    int startAccount=200,endAccount=1000;

    std::vector<std::thread> threads;
    auto startTime = std::chrono::high_resolution_clock::now();
    for (int accountNum = startAccount; accountNum <= endAccount; ++accountNum) {
        threads.push_back(std::thread(registerAccount, accountNum));
    }
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    int totalAccounts = (endAccount - startAccount + 1);
    double requestsPerSecond = static_cast<double>(totalAccounts) / elapsedTime;
    std::cout << "Total accounts processed: " << totalAccounts << std::endl;
    std::cout << "Total time (seconds): " << elapsedTime << std::endl;
    std::cout << "Requests per second: " << requestsPerSecond << std::endl;
    return 0;
}



#if 0
int test()
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
    string account = "201";
    string username = "201";
    string password = "254003";
    Json::Value regisMsg;
    regisMsg["account"] = account;
    regisMsg["username"]=username;
    regisMsg["password"]= password;
    Json::StreamWriterBuilder writer;
    std::string regisMsgstr = Json::writeString(writer, regisMsg);
    // string loginMsgstr = loginMsg.toStyledString();
    //报文头部
    string headstr = encoder.encode(REGISTER,stoi(account),TEXT,regisMsgstr.length());

    write(fd, headstr.c_str(), headstr.length());

    if(regisMsgstr.length()!=0)
    {
        write(fd, regisMsgstr.c_str(),regisMsgstr.length());
        std::cout<<"发送成功，数据总大小:"<<regisMsgstr.length()<<std::endl;
    }

    HeadData hd(fd);
    int datalength = hd.getDataLength(); 
    char buf[256];
    read(fd,buf, datalength);
    string recvMsg = string(buf,datalength);
    Json::Reader jsonReader;
    Json::Value msg;
    jsonReader.parse(recvMsg, msg);

    if(msg["status"]==REGISTER_SUCCESS){
        cout<<account<<"注册成功"<<endl;
    }
}
#endif

