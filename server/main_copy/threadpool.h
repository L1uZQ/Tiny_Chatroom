#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<pthread.h>
#include<list>
#include"locker.h"
#include<exception>
#include<cstdio>
#include"./Service/DataProcesser.h"
#include<jsoncpp/json/json.h>
#include"ProtocolHead/HeadData.h"
#include"Service/DataProcesser.h"
#include "Service/UserService.h"
#include "Service/Online.h"
#include "config/server_config.h"

//定义一个结构体，用于传入线程池的工作队列对任务进行处理
struct roominfo{
    UserService* us;
    Online* online;
    epoll_event *ev{};
    HeadData* hd;
    int fd;
    int epfd;
};

//线程池类，定义成模板类为了代码复用
template<typename T>
class threadpool{
public:
    threadpool(int thread_number = 4, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);

private:
    static void * worker(void * arg);
    void run();
private:
    //线程的数量
    int m_thread_number;
    // 线程池数组，大小为 m_thread_number
    pthread_t * m_threads;
    //请求队列中最多允许的等待处理的请求数量
    int m_max_requests;
    //请求队列
    std::list <T *> m_workqueue;
    //互斥锁
    locker m_queuelocker;
    //信号量，用来判断是否有任务需要处理
    sem m_queuestat;
    //是否结束线程
    bool m_stop;
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests):
    m_thread_number(thread_number), m_max_requests(max_requests),
    m_stop(false), m_threads(NULL) //列表初始化
{

    if((thread_number <= 0) || max_requests<=0){   
        throw std:: exception();
    }

    //创建数组
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std:: exception();
    }
    //创建thread_number个线程，并将他们设置为线程脱离
    for(int i=0 ;i<thread_number; ++i){
        printf("正在创建第 %d 个线程\n", i);

        if(pthread_create(m_threads + i, NULL, worker, this) != 0){
            delete [] m_threads;
            throw std:: exception();
        }

        //非阻塞：将该子线程的状态设置为detached,则该线程运行结束后会自动释放所有资源。
        if(pthread_detach(m_threads[i])){  
            delete [] m_threads;
            throw std:: exception();
        }
    }

}

template<typename T>
threadpool<T>::~threadpool(){
    delete [] m_threads;
    m_stop = true;
}

template<typename T>
//生产者消费者模型
bool threadpool<T>::append(T * request){
    cout<<"*****************************"<<endl;
    m_queuelocker.lock();
    if(m_workqueue.size()> m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post(); //信号量增加一个
    return true;
}


template<typename T>
//worker静态函数，不能访问非静态成员
void* threadpool<T>::worker(void * arg){
    threadpool * pool = (threadpool *) arg;
    pool->run();
    return pool;
}

//线程池运行函数，需要一直循环，直到m_stop为false
template<typename T>
void threadpool<T>::run(){
    while(!m_stop){ //当线程状态不为停止时，一直运行
        m_queuestat.wait(); //P操作
        m_queuelocker.lock();//上锁
        
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }

        T* request = m_workqueue.front();//获取第一个任务
        m_workqueue.pop_front(); //出队
        m_queuelocker.unlock();  //解锁

        if(!request){
            continue;
        }
        process(request);
        // request->process(); //线程池运行函数
    }
}

//fd  online   us(验证密码)  ev  epfd
void process(roominfo* work){
    cout<<"****work->fd***:"<<work->fd<<endl;
    // HeadData hd(work->fd);

    unsigned int protocolId = work->hd->getProtocolId();
    unsigned int account = work->hd->getAccount();
    unsigned int dataType = work->hd->getDataType();
    unsigned int dataLength = work->hd->getDataLength();
    cout<<"***work->hd->protocolId***"<<protocolId<<endl;
    DataProcesser dp;
    switch (protocolId) {
        case LOGIN: {
            string loginMsg = dp.readTextContent(work->fd, dataLength);
            cout<<"登录信息: "<<loginMsg<<endl;
            Json::Reader jsonReader;
            Json::Value msg;
            jsonReader.parse(loginMsg, msg);
            cout<<"测试打印消息："<<msg<<endl;
            string account = msg["account"].asString();
            cout<<"account: "<<account<<endl;
            string password = msg["password"].asString();
            pair<int, string> user = work->us->checkLogin(account, password);
            cout<<"user: "<<user.first<<" "<<user.second<<endl;
            Json::Value loginResult;
            
            //登录成功
            if (user.first != 0) {
                if (work->online->isLogin(user.first)) {
                    loginResult["status"] = LOGIN_EXIST;
                } else {
                    work->online->appendUser(user);
                    work->online->appendWriteFd(user.first, work->fd);
                    loginResult["status"] = LOGIN_SUCCESS;
                    loginResult["username"] = user.second;
                }
            }
            
            //失败
            else {
                loginResult["status"] = LOGIN_FAIL;
            }
            string loginResultStr = loginResult.toStyledString();
            dp.writeMsg(work->fd, 0, loginResult.toStyledString(), LOGIN);
        }
            break;
            
        case REGISTER: {
            string registerMsg = dp.readTextContent(work->fd, dataLength);
            Json::Reader jsonReader;
            Json::Value registerResult;
            Json::Value msg;
            jsonReader.parse(registerMsg, msg);
            string account = msg["account"].asString();
            string username = msg["username"].asString();
            string password = msg["password"].asString();
            if (work->us->isRegistered(account) || !work->us->registerUser(account, username, password)) {
                registerResult["status"] = REGISTER_FAIL;
            } else {
                registerResult["status"] = REGISTER_SUCCESS;
            }
            dp.writeMsg(work->fd, 0, registerResult.toStyledString(), REGISTER);
        }
            break;

        case SEND: {
            string baseMsg = work->online->getUserName(account) + "(" + to_string(account) + ")说:";
            if (dataType == TEXT) {
                dp.writeTextToAllUser(work->online->getAllReadFd(), account, baseMsg);
                string content = dp.readTextContent(work->fd, dataLength);
                dp.writeTextToAllUser(work->online->getAllReadFd(), account, content);
            } else if (dataType == IMAGE) {
                string imagePath = dp.readImageContent(work->fd, dataLength);
                if (dp.getFileLength(imagePath) == dataLength) {
                    dp.writeTextToAllUser(work->online->getAllReadFd(), account, baseMsg);
                    dp.writeImageToAllUser(work->online->getAllReadFd(), account, imagePath);
                } else {
                    work->ev->data.fd = work->fd;
                    work->ev->events = EPOLLIN;
                    epoll_ctl(work->epfd, EPOLL_CTL_DEL, work->fd, work->ev);
                    close(work->fd);
                    close(work->online->getReadFd(work->fd));
                    string logoutMsg =
                            work->online->getUserName(account) + "(" + to_string(account) + ")" + "离开了聊天室!";
                    work->online->removeUser(account);
                    vector<int> fds = work->online->getAllReadFd();
                    if (!fds.empty()) {
                        dp.writeTextToAllUser(fds, account, logoutMsg, NOTICE);
                        dp.writeTextToAllUser(fds, 0, work->online->getOnlineListStr(), ONLINELIST);
                    }
                }
            }
        }
            break;
        case READ: {
            work->ev->data.fd = work->fd;
            work->ev->events = EPOLLIN;
            epoll_ctl(work->epfd, EPOLL_CTL_DEL, work->fd, work->ev);
            work->online->appendReadFd(account, work->fd);
            string loginMsg = work->online->getUserName(account) + "(" + to_string(account) + ")" + "走进了聊天室!";
            dp.writeTextToAllUser(work->online->getAllReadFd(), account, loginMsg, NOTICE);
            dp.writeTextToAllUser(work->online->getAllReadFd(), account, work->online->getOnlineListStr(), ONLINELIST);
        }
            break;
        case LOGOUT: {
            sleep(1);
            work->ev->data.fd = work->fd;
            work->ev->events = EPOLLIN;
            epoll_ctl(work->epfd, EPOLL_CTL_DEL, work->fd, work->ev);
            close(work->fd);
            close(work->online->getReadFd(work->fd));
            string logoutMsg = work->online->getUserName(account) + "(" + to_string(account) + ")" + "离开了聊天室!";
            work->online->removeUser(account);
            vector<int> fds = work->online->getAllReadFd();
            cout << "当前在线人数:" << fds.size() << endl;
            if (!fds.empty()) {
                dp.writeTextToAllUser(fds, account, logoutMsg, NOTICE);
                dp.writeTextToAllUser(fds, 0, work->online->getOnlineListStr(), ONLINELIST);
            }
        }
            break;
        case CLOSE: {
            sleep(1);
            work->ev->data.fd = work->fd;
            work->ev->events = EPOLLIN;
            epoll_ctl(work->epfd, EPOLL_CTL_DEL, work->fd, work->ev);
            close(work->fd);
        }
            break;       
    }
    if(work!= nullptr)
        delete work;
}


#endif