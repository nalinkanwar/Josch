#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "sys/epoll.h"
#include <string>
#include <thread>
#include <list>
#include <chrono>

#include "tlv.h"
#include "josch.h"
#include "conn_handler.h"

class client_handler
{
    protected:
        std::thread th;
        std::atomic<bool> quit;        
        Josch *j;

        std::string fpath;
        int lfd, epfd;
        std::list<class conn_handler<class client_handler>> cl_list;
    public:
        client_handler(Josch *tmpj);
        ~client_handler();
        bool init(const std::string &fp);

        void handle_conns();
        void handle_listener();

        friend tlv_types command_handler(client_handler& ch, char* ptr, int length);

        void join();
        void die();
};

#endif // CLIENT_HANDLER_H
