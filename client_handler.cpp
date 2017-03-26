#include <iostream>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cstdlib>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "client_handler.h"

extern std::atomic<bool> main_quit;

client_handler::client_handler(Josch *tmpj) {
    this->j = tmpj;
    this->quit = false;
    this->lfd = -1;
    this->epfd = -1;
}

client_handler::~client_handler() {

    if(!this->fpath.empty()) {
        unlink(this->fpath.c_str());
    }

    this->die();
    if(this->th.joinable()) {
        this->th.join();
    }

    if(this->lfd != -1) {
        close(this->lfd);
    }

    if(this->epfd != -1) {
        close(this->epfd);
    }
}

void client_handler::join() {
    if(this->th.joinable()) {
        this->th.join();
    }
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
        return false;
    }

    sun.sun_family = AF_UNIX;
    memcpy(sun.sun_path, fpath.c_str(), fpath.length() + 1);

    if(bind(this->lfd, (sockaddr *)&sun, sizeof(sun)) == -1) {
        LOG<<"Failed to bind because "<<std::strerror(errno)<<std::endl;
        close(this->lfd);
        return false;
    }

    if(listen(this->lfd, 10) == -1) {
        LOG<<"Failed to start listener because "<<std::strerror(errno)<<std::endl;
        close(this->lfd);
        return false;
    }

    this->epfd = epoll_create1(0);
    if(this->epfd == -1) {
        LOG<<"Failed to initialize epoll because "<<std::strerror(errno)<<std::endl;
        close(this->lfd);
        return false;
    }

    int flags = fcntl(this->lfd, F_GETFL, 0);
    fcntl(this->lfd, F_SETFL, flags | O_NONBLOCK);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = this->lfd;
    epoll_ctl(this->epfd, EPOLL_CTL_ADD, this->lfd, &ev);

    LOG<<"Listening on "<<this->fpath<<" with fd: "<<this->lfd<<std::endl;

    this->th = std::thread(&client_handler::handle_conns, this);
}

void client_handler::handle_listener() {

    int cfd = accept(this->lfd, NULL, NULL);
    epoll_event ev;

    if(cfd == -1) {
        LOG<<"Accept failed due to "<<std::strerror(errno)<<std::endl;
        return;
    }

    int flags = fcntl(cfd, F_GETFL, 0);
    fcntl(cfd, F_SETFL, flags | O_NONBLOCK);

    this->cl_list.push_back(conn_handler<client_handler>(cfd));

    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;

    epoll_ctl(this->epfd, EPOLL_CTL_ADD, cfd, &ev);

    LOG<<" New client connected: "<<cfd<<std::endl;
}

tlv_types command_handler(client_handler& ch, char* ptr, int length) {

    //LOG<<"Command_handler called"<<std::endl;
    struct tlv *tptr = (struct tlv*) ptr;
    switch(tptr->type) {
        case TLV_REGISTER_JOB:
        {
            LOG<<"Got Register Job :";
            std::cout.write(tptr->value, tptr->length);
            std::cout<<std::endl;

            std::string str;
            str.append(tptr->value, tptr->length);
            auto it = str.find_last_of(',');

            std::string cmd = str.substr(0, it);
            int interval = std::strtol(str.substr(it + 1).c_str(), NULL, 10);

            bool ret = ch.j->register_job(cmd, interval);

            if(ret == true) {
                LOG<<"TLV_SUCCESS"<<std::endl;
                return TLV_SUCCESS;
            } else {
                LOG<<"TLV_FAILURE"<<std::endl;
                return TLV_FAILURE;
            }
        }
            break;
        case TLV_UNREGISTER_JOB:
        {
            LOG<<"Got Unregister Job with length "<<tptr->length<<" : ";
            std::cout.write(tptr->value, tptr->length);
            std::cout<<std::endl;

            std::string str;
            str.append(tptr->value, tptr->length);
            auto it = str.find_last_of(',');

            std::string cmd = str.substr(0, it);
            int interval = std::strtol(str.substr(it + 1).c_str(), NULL, 10);

            bool ret = ch.j->unregister_job(cmd, interval);

            if(ret == true) {
                LOG<<"TLV_SUCCESS"<<std::endl;
                return TLV_SUCCESS;
            } else {
                LOG<<"TLV_FAILURE"<<std::endl;
                return TLV_FAILURE;
            }
        }
            break;
        case TLV_LIST_JOBS:
            break;
        default:
            break;
    }
}

bool write_handler(client_handler& ch, char* ptr, int length) {

    return true;
}


void client_handler::handle_conns() {

    int nevents;
    epoll_event ev[10];

    while(this->quit == false) {

        nevents = epoll_wait(this->epfd, ev, 10, 1000);

        for(int i = 0; i < nevents; i++) {
            if(ev[i].data.fd == this->lfd) {
                this->handle_listener();
            } else {
                auto it = std::find_if(this->cl_list.begin(), this->cl_list.end(), [&ev, &i] (const conn_handler<client_handler> &cl) {
                    if(ev[i].data.fd == cl.get_fd()) {
                        return true;
                    }
                    return false;
                });

                assert(it != this->cl_list.end());

                if(ev[i].events & EPOLLIN) {
                    if((*it).reader((*this), command_handler) == false) {
                        close(ev[i].data.fd);
                        std::remove_if(this->cl_list.begin(), this->cl_list.end(), [&ev, &i] (const conn_handler<client_handler> &cl) {
                            if(ev[i].data.fd == cl.get_fd()) {
                                return true;
                            }
                            return false;
                        });
                        continue;
                    }
                }

                if(ev[i].events & EPOLLOUT || (*it).ready_to_write()) {
                    (*it).writer((*this), write_handler);
                }
            }
        }
    }
}
