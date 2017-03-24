#include <iostream>
#include <cstring>
#include <algorithm>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>

#include "client_handler.h"

extern std::atomic<bool> main_quit;

client_handler::client_handler(Josch *tmpj) {
    this->j = tmpj;
    this->quit = false;
}

client_handler::~client_handler() {
    close(this->lfd);
    unlink(this->fpath.c_str());
}

void client_handler::join() {
    this->th.join();
}

void client_handler::die() {
    this->quit = true;
}

bool client_handler::init(const std::string &fpath) {
    struct sockaddr_un sun;

    this->fpath = fpath;

    this->lfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(this->lfd == -1) {
        LOG<<"Socket creation failed because "<<std::strerror(errno)<<std::endl;
        //@FIXME throw
    }

    sun.sun_family = AF_UNIX;
    memcpy(sun.sun_path, fpath.c_str(), fpath.length() + 1);

    if(bind(this->lfd, (sockaddr *)&sun, sizeof(sun)) == -1) {
        LOG<<"Failed to bind because "<<std::strerror(errno)<<std::endl;
        //@FIXME throw
    }

    if(listen(this->lfd, 10) == -1) {
        LOG<<"Failed to start listener because "<<std::strerror(errno)<<std::endl;
        //@FIXME throw
    }

    this->epfd = epoll_create1(0);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = this->lfd;
    epoll_ctl(this->epfd, EPOLL_CTL_ADD, this->lfd, &ev);

    LOG<<"Listening on "<<this->fpath<<" with fd: "<<this->lfd<<std::endl;

    th = std::thread(&client_handler::handle_conns, this);
}

bool client_handler::handle_listener(epoll_event & ev) {

    int new_cfd;

    if(ev.events || EPOLLIN) {
        new_cfd = accept(this->lfd, NULL, 0);
        if(new_cfd == -1) {
            LOG<<"Accept failed because"<<std::strerror(errno)<<std::endl;
            //@FIXME throw
        }
        LOG<<"New client connected: "<<new_cfd<<std::endl;

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = new_cfd;

        epoll_ctl(this->epfd, EPOLL_CTL_ADD, new_cfd, &ev);

        struct client c;
        c.fd = new_cfd;
        c.filled = c.processed = 0;
        c.size = BUFFSIZE;
        c.state = STATE_RECVING;

        this->clist.push_back(c);
    }
}


void client_handler::close_client(int fd) {
    close(fd);
    this->clist.erase((std::remove_if(this->clist.begin(), this->clist.end(), [&fd] (const struct client &tc) {
        if (fd == tc.fd) {
            return true;
        }
        return false;
    })), this->clist.end());
}

bool client_handler::handle_client(epoll_event &ev) {

    int ret;

    if((ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
        LOG<<"Client "<<ev.data.fd<<" closed connection"<<std::endl;
        this->close_client(ev.data.fd);
        return false;
    }

    /* bad sequential lookup; @FIXME */
    auto ait = this->clist.begin();
    while(ait != this->clist.end()) {
        if((*ait).fd == ev.data.fd) {
            break;
        }
        ait++;
    }

    if(ait == this->clist.end()) {
        //@FIXME handle error;
        return false;
    }

    auto &cl = (*ait);

    if(ev.events & EPOLLOUT) {
        if(cl.state != STATE_SENDING) {
            LOG<<"Client not following protocol!"<<std::endl;
            this->close_client(cl.fd);
            return false;
        }
        ret = write(cl.fd, cl.iobuf + cl.processed, cl.filled - cl.processed);
        switch(ret) {
            case 0:
            case -1:
                this->close_client(cl.fd);
                break;
            default:
                cl.processed += ret;
                if(cl.processed == cl.filled) {
                    this->close_client(cl.fd);
                    return true;
                }
                break;
        }
    }

    if(ev.events & EPOLLIN) {
        if(cl.state != STATE_RECVING) {
            LOG<<"Client not following protocol!"<<std::endl;
            this->close_client(cl.fd);
            return false;
        }

        LOG<<"Going to read"<<std::endl;
        ret = read(cl.fd, (cl.iobuf + cl.filled), (cl.size - cl.filled));
        switch(ret) {
            case 0:
                this->close_client(cl.fd);
                break;
            case -1:
                if(errno == EAGAIN) {
                    return true;
                }
                this->close_client(cl.fd);
                return false;
                break;
            default:
                LOG<<"Got "<<ret<<" bytes <"<<sizeof(struct tlv)<<std::endl;
                cl.filled += ret;
                if((cl.filled - cl.processed) >= sizeof(struct tlv)) {
                    struct tlv* tptr = (struct tlv*)(cl.iobuf + cl.processed);

                    LOG<<"Got t: "<<tptr->type<<" l: "<<tptr->length<<" v: "<<((char*)tptr + sizeof(struct tlv))<<std::endl;

                    switch (tptr->type) {
                        case TLV_REGISTER_JOB:
                            if((cl.filled - cl.processed) >= (sizeof(struct tlv) + tptr->length)) {
                                std::string s = "";
                                s.append(((char*)tptr + sizeof(struct tlv)), tptr->length);
                                LOG<<"Got string "<<s<<" "<<std::endl;

                                size_t comma = s.find_last_of(',');

                                std::string cmd = s.substr(0, comma);
                                int ival = std::strtol(s.substr(comma + 1).c_str(), NULL, 10);

                                LOG<<"Got cmd "<<cmd<<" with ival "<<ival<<std::endl;

                                cl.processed += (sizeof(struct tlv) + tptr->length);

                                cl.state = STATE_SENDING;
                                struct epoll_event ev;
                                ev.events = EPOLLIN | EPOLLOUT;
                                ev.data.fd = cl.fd;
                                epoll_ctl(this->epfd, EPOLL_CTL_MOD, cl.fd, &ev);

                                this->j->register_job(cmd, ival);

                                tptr = (struct tlv*)cl.iobuf;
                                tptr->type = TLV_SUCCESS;
                                tptr->length = 0;

                                cl.filled = sizeof(struct tlv);
                                cl.processed = 0;
                            }
                            break;
                        case TLV_UNREGISTER_JOB:
                            if((cl.filled - cl.processed) >= (sizeof(struct tlv) + tptr->length)) {
                                std::string s = "";
                                s.append(((char*)tptr + sizeof(struct tlv)), tptr->length);
                                LOG<<"Got string "<<s<<" "<<std::endl;

                                size_t comma = s.find_last_of(',');

                                std::string cmd = s.substr(0, comma);
                                int ival = std::strtol(s.substr(comma + 1).c_str(), NULL, 10);

                                LOG<<"Got cmd "<<cmd<<" with ival "<<ival<<std::endl;

                                cl.processed += (sizeof(struct tlv) + tptr->length);
                                cl.state = STATE_SENDING;

                                struct epoll_event ev;
                                ev.events = EPOLLIN | EPOLLOUT;
                                ev.data.fd = cl.fd;
                                epoll_ctl(this->epfd, EPOLL_CTL_MOD, cl.fd, &ev);

                                this->j->unregister_job(cmd, ival);

                                tptr = (struct tlv*)cl.iobuf;
                                tptr->type = TLV_SUCCESS;
                                tptr->length = 0;

                                cl.filled = sizeof(struct tlv);
                                cl.processed = 0;
                            }
                            break;
                        case TLV_LIST_JOBS:
                            break;
                        default:
                            break;
                    }

                }
                break;
        }
    }
    return true;
}

bool client_handler::handle_conns() {

    int nevents;
    epoll_event evs[10];

    LOG<<"Start client_handler"<<std::endl;
    while(this->quit == false) {
        LOG<<"Waiting for event"<<std::endl;
        nevents = epoll_wait(this->epfd, evs, 10, 1000);
        if(nevents == -1) {
            LOG<<"epoll_wait failed due to "<<std::strerror(errno)<<std::endl;
            continue;
        }

        for(int i = 0; i < nevents; i++) {
            LOG<<"Handling event for "<<evs[i].data.fd<<std::endl;

            if(evs[i].data.fd == this->lfd) {
                this->handle_listener(evs[i]);
            } else {
                this->handle_client(evs[i]);
            }
        }
    }
    LOG<<"End client_handler"<<std::endl;
}
