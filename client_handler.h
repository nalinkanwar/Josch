#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "sys/epoll.h"
#include <string>
#include <thread>
#include <list>
#include <chrono>

#include "tlv.h"
#include "josch.h"

#define LOG std::cout<<"["<<std::this_thread::get_id()<<"] "
#define BUFFSIZE 8196

enum client_states {
    STATE_RECVING,
    STATE_SEND_RESP,
    STATE_SEND_LIST
};

struct client {
    int fd;
    int state;
    uint32_t size;
    uint32_t filled;
    uint32_t processed;

    int interval;
    std::string cmd;
    bool write_ready;

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
        bool process_data(struct tlv *tptr, client &cl);
        bool write_handler(struct client &cl);
        bool read_handler(struct client &cl);
        void set_state_sending(struct tlv *tptr, struct client &cl, int type);
        bool send_list_handler(struct client &cl);
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
