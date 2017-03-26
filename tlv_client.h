#ifndef TLV_CLIENT_H
#define TLV_CLIENT_H

#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include "tlv.h"

#include "conn_handler.h"

#define DEFAULT_FPATH "/tmp/josch_default.sock"

#define LOG std::cout<<"["<<std::this_thread::get_id()<<"] "\

class tlv_client
{
    int fd;
    std::string fpath;
    class conn_handler<class tlv_client> conn;

    public:
        tlv_client();
        ~tlv_client();
        bool init(const std::string &f);

        bool sendcmd(tlv_types type);
        bool sendcmd(tlv_types type, std::string& cmd, int interval = 0);

        /* dummy */
        void get_list_of_jobs();
        std::string& get_joblist();
};

#endif // TLV_CLIENT_H

