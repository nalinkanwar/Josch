#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

#include "conn_handler.h"
#include "tlv.h"

/* Force template generations */
class client_handler;
class tlv_client;
template class conn_handler<client_handler>;
template class conn_handler<tlv_client>;

template<class T>
conn_handler<T>::conn_handler()
{
    this->iob.size = this->iob.filled = this->iob.processed = 0;
    this->iob.buf = nullptr;
    this->write_ready = true;
}

template<class T>
conn_handler<T>::conn_handler(int fd)
{
    this->fd = fd;
    this->iob.size = this->iob.filled = this->iob.processed = 0;
    this->iob.buf = nullptr;
    this->write_ready = true;
}

template<class T>
conn_handler<T>::~conn_handler() {
    if(this->iob.buf != nullptr) {
        free(this->iob.buf);
    }
}

template<class T>
void conn_handler<T>::set_fd(int fd) {
    this->fd = fd;
}

template<class T>
int conn_handler<T>::get_fd() const{
    return this->fd;
}


template<class T>
void conn_handler<T>::reset_iob() {
    this->iob.filled = this->iob.processed = 0;
}

template<class T>
int conn_handler<T>::ready_to_write() const {
    return this->write_ready;
}

template<class T>
bool conn_handler<T>::write_to_peer(char* cmd, int len) {

    if((this->iob.size - this->iob.filled) < len) {
        char *tmp;
        tmp = (char *)realloc(this->iob.buf, this->iob.size + len);
        if(tmp == NULL) {
            LOG<<"Unable to allocate memory due to: "<<std::strerror(errno)<<std::endl;
            return false;
        }
        this->iob.buf = tmp;
        this->iob.size = this->iob.size + len;
    }
    memcpy(this->iob.buf + this->iob.filled, cmd, len);
    this->iob.filled += len;
    return true;
}

template<class T>
bool conn_handler<T>::write_to_peer(std::string& cmd) {

    if((this->iob.size - this->iob.filled) < cmd.length()) {
        char *tmp;
        tmp = (char *)realloc(this->iob.buf, this->iob.size + cmd.length());
        if(tmp == NULL) {
            LOG<<"Unable to allocate memory due to: "<<std::strerror(errno)<<std::endl;
            return false;
        }
        this->iob.buf = tmp;
        this->iob.size = this->iob.size + cmd.length();
    }
    memcpy(this->iob.buf + this->iob.filled, cmd.c_str(), cmd.length());
    *(this->iob.buf + this->iob.filled + cmd.length()) = '\0';

    this->iob.filled += cmd.length();
    this->iob.processed = 0;
    return true;
}

template<class T>
bool conn_handler<T>::reader(T &ch, tlv_types (*process_data)(T&, char *, int)) {
    int ret;

    LOG<<"READER called"<<std::endl;
    while(1) {
        if(this->iob.size == this->iob.filled) {
            char *tmp;
            tmp = (char *)realloc(this->iob.buf, this->iob.size + BUFFSIZE);
            if(tmp == NULL) {
                LOG<<"Unable to allocate memory due to: "<<std::strerror(errno)<<std::endl;
                return false;
            }

            this->iob.buf = tmp;
            this->iob.size = this->iob.size + BUFFSIZE;
            LOG<<"Buffer expanded to "<<this->iob.size<<std::endl;
        }
        errno = 0;
        ret = read(this->fd,
                    this->iob.buf + this->iob.filled,
                        this->iob.size - this->iob.filled);
        switch(ret) {
            case -1:
                if(errno == EAGAIN) {
                    LOG<<"EAGAIN"<<std::endl;
                    return true;
                }
                LOG<<"Read failed due to:"<<std::strerror(errno)<<std::endl;
                return false;
            case 0:
                LOG<<"Remote peer closed connection"<<std::endl;
                return false;
                break;
            default:
                LOG<<"Read "<<ret<<" bytes"<<std::endl;
                this->iob.filled += ret;
                if((this->iob.filled - this->iob.processed) >= sizeof (struct tlv)) {
                    struct tlv* tptr = (struct tlv*)(this->iob.buf + this->iob.processed);

                    LOG<<"Got tlv t:"<<tptr->type<<" l: "<<tptr->length<<std::endl;

                    if((this->iob.filled - this->iob.processed) >= (sizeof(struct tlv) + tptr->length)) {
                        //DO PROCESSING
                        tlv_types resp = process_data(ch, (char *)tptr, sizeof(struct tlv) + tptr->length);

                        switch(resp) {
                            case TLV_SUCCESS:
                                {
                                    struct tlv t;
                                    char *tptr = (char *)&t;
                                    t.type = TLV_SUCCESS;
                                    t.length = 0;
                                    t.last = 1;

                                    this->iob.filled = this->iob.processed = 0;
                                    if(this->write_to_peer(tptr, sizeof(struct tlv)) == false) {
                                        return false;
                                    }
                                    return true;
                                }
                                break;
                            case TLV_FAILURE:
                                {
                                    struct tlv t;
                                    char *tptr = (char*)&t;
                                    t.type = TLV_FAILURE;
                                    t.length = 0;
                                    t.last = 1;

                                    this->iob.filled = this->iob.processed = 0;
                                    if(this->write_to_peer(tptr, sizeof(struct tlv)) == false) {
                                        return false;
                                    }
                                    return true;
                                }
                                break;
                            default:
                                /* do nothing */
                                break;
                        }

                        this->iob.processed += (sizeof(struct tlv) + tptr->length);
                        LOG<<"Processed "<<this->iob.processed<<" bytes"<<std::endl;
                    }
                }
                break;
        }
    }
    LOG<<"READER end"<<std::endl;
    return true;
}

template<class T>
bool conn_handler<T>::writer(T &ch, bool (*process_data)(T &, char *, int)) {

    int ret;

    LOG<<"WRITER called"<<std::endl;
    this->write_ready = true;

    if(this->iob.filled <= this->iob.processed) {
        LOG<<"Nothing to write! "<<this->iob.filled<< " <= "<<this->iob.processed<<std::endl;
        return true;
    }

    errno = 0;
    ret = write(this->fd, this->iob.buf + this->iob.processed, (this->iob.filled - this->iob.processed));

    switch(ret) {
        case -1:
        case 0:
            LOG<<"Write failed due to:"<<std::strerror(errno)<<std::endl;
            return false;
            break;
        default:
            {
                LOG<<"Wrote "<<ret<<" bytes"<<std::endl;
                //DO PROCESSING?
                if(process_data(ch, this->iob.buf + this->iob.processed, ret) == false) {
                    this->iob.filled = this->iob.processed = 0;
                    return true;
                }
                this->iob.processed += ret;
                LOG<<"Processed total "<<this->iob.processed<<" bytes"<<std::endl;
            }
            break;
    }
    LOG<<"WRITER end"<<std::endl;
    return false;
}
