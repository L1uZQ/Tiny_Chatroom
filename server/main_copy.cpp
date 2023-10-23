#include<iostream>
#include<string>
#include<vector>
#include<algorithm>
#include<arpa/inet.h>
#include<cstdlib>
#include<signal.h>
#include<unistd.h>
#include<sys/epoll.h>
#include<cstring>
#include<jsoncpp/json/json.h>
#include"ProtocolHead/HeadData.h"
#include"Service/DataProcesser.h"
#include "Service/UserService.h"
#include "Service/Online.h"
#include "config/server_config.h"
#include "./threadpool.h"
#include<unordered_map>

const int MAX_FD=1000;


using namespace std;

//添加信号捕捉
void addsig(int sig, void(handler)(int)){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

unordered_map<int,bool>sign;
// std::unordered_map<int, bool> roominfo::externalSign;
std::unordered_map<int, bool> roominfo::sign;


// void roominfo::associateSign(const std::unordered_map<int, bool>& externalSign) {
//     unordered_map<int, bool>& roominfo::sign = externalSign;
// }


int main() {
    // roominfo::associateSign(sign);
    // 创建线程池，初始化线程池
    threadpool<roominfo> * pool = NULL;
    try{
        pool = new threadpool<roominfo>; //创建http_conn类型的线程池
    }catch(...){
        exit(-1);
    }

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{}, clientAddr{};
    int opt = 1;
    if (-1 == setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        cout << "setsockopt fail" << endl;
        exit(-1);
    }//设置端口复用
    int epfd = epoll_create(MAX_CONNECTIONS);
    epoll_event ev{}, events[MAX_CONNECTIONS];
    ev.data.fd = lfd;
    ev.events = EPOLLIN;
    if (-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev)) {
        cout << "epoll_ctl fail" << endl;
        exit(-1);
    }
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_family = AF_INET;
    // inet_pton(AF_INET, HOST, &serverAddr.sin_addr);
    serverAddr.sin_addr.s_addr = INADDR_ANY; 

    if (-1 == bind(lfd, (sockaddr *) &serverAddr, sizeof(serverAddr))) {
        cout << "bind fail" << endl;
        exit(-1);
    }

    if (-1 == listen(lfd, MAX_CONNECTIONS)) {
        cout << "listen fail" << endl;
        exit(-1);
    }
    cout << "listening..." << endl;

    char ipAddress[BUFSIZ];
    UserService us;
    Online online;
    
    roominfo *work= new roominfo[MAX_FD];

    while (true) {
        //调用epoll_wait获取请求数
        int nready = epoll_wait(epfd, events, MAX_CONNECTIONS, -1);
        if (nready < 0) {
            cout << "epoll_wait error" << endl;
            exit(-1);
        }
        // cout << "收到" << nready << "个请求" << endl;
        for (int i = 0; i < nready; i++) {
            int fd = events[i].data.fd;
            if (fd == lfd) { //如果是新的连接
                socklen_t len = sizeof(clientAddr);
                int cfd = accept(lfd, (sockaddr *) &clientAddr, &len);
                ev.data.fd = cfd;
                ev.events = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
                inet_ntop(AF_INET, &clientAddr.sin_addr, ipAddress, sizeof(clientAddr));
                //设置超时read
                struct timeval timeout = {1, 0};
                setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(struct timeval));
            } else if (events[i].events & EPOLLIN && roominfo::sign[fd]==false ) { 
                //如果这个文件描述符上有可读的事件
                //应该先读取文件描述符，再压到线程池里面
                //主线程将fd传到工作线程之后，主线程就不能管这个fd的状态变化了
                // HeadData hd(fd);
                roominfo::sign[fd]=true;
                // unsigned int protocolId = hd.getProtocolId();
                // cout<<"protocolId :"<<protocolId;
                //每次循环创建一个（还是要提前创建好）
                // http_conn * users = new http_conn[MAX_FD];
                work[fd].us=&us;
                work[fd].online=&online;
                work[fd].fd=fd;
                work[fd].ev=&ev;

                // work[fd].us=us;
                // work[fd].online=online;
                // work[fd].fd=fd;
                // work[fd].ev=ev;
                // work[fd].hd=&hd;
                //work->ev.data.fd = fd;
                work[fd].epfd=epfd;
                pool->append(work+fd);
            }
        }
    }
    // delete work;
    close(lfd);
    return 0;
}
