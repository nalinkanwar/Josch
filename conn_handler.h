#ifndef CONN_HANDLER_H
#define CONN_HANDLER_H

#include "tlv.h"
#include <thread>

#define BUFFSIZE 4096
//#define LOG std::cout<<"["<<std::this_thread::get_id()<<"] "

struct iobuf {
    int size;
    int filled;
    int processed;
    char *buf;
};

template <class T>
class conn_handler
{
    private:
        int fd;
        bool write_ready;
        struct iobuf iob;
        T* cl;
    public:
        conn_handler();
        conn_handler(int fd);
        ~conn_handler();

        bool write_to_peer(std::string& cmd);
        bool write_to_peer(char* cmd, int len);

        void reset_iob();
        void set_fd(int fd);
        int get_fd() const;
        int ready_to_write() const;
        void set_cl(T*);
        bool reader(T &ch, tlv_types (*process_data)(T &, char *, int));
        bool writer(T &ch, bool (*process_data)(T &, char *, int));
};

#endif // CONN_HANDLER_H
