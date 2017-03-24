#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "sys/epoll.h"
#include <string>
#include <thread>
#include <list>

#include "tlv.h"
#include "josch.h"

#define LOG std::cout<<"["<<std::this_thread::get_id()<<"] "
#define BUFFSIZE 8196

enum client_states {
    STATE_RECVING,
    STATE_SENDING
};

struct client {
    int fd;
    int state;
    uint32_t size;
    uint32_t filled;
    uint32_t processed;
    bool write_pending;
    char iobuf[BUFFSIZE];
};

class client_handler
{
    private:
        std::thread th;
        int lfd;
        int epfd;
        std::list<struct client> clist;
        std::string fpath;
        std::atomic<bool> quit;
        Josch *j;
        void close_client(int fd);
    public:
        client_handler(Josch *tmpj);
        ~client_handler();
        bool init(const std::string &fpath);
        bool handle_conns();
        bool handle_listener(epoll_event &ev);
        bool handle_client(epoll_event &ev);

        void join();
        void die();
};

#endif // CLIENT_HANDLER_H
