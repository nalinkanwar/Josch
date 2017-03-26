#include <iostream>
#include <thread>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#include <getopt.h>

#include "job.h"
#include "josch.h"
#include "tlv_client.h"
#include "client_handler.h"

using namespace std;

uint64_t JobId::jobid_counter = 0;

std::atomic<bool> main_quit;

/* reap the zombie childrens */
void proc_exit(int signal) {
    int wstatus, ret;
    while(1) {
        ret = waitpid(0, &wstatus, WNOHANG);
        if(ret == 0 || ret == -1) {
            break;
        }
    }
}

void handle_quit(int signal) {
    main_quit = true;
}

/* Pending Tasks
 * 1. get number of hw threads (with hw_concurrency/sysinfo?) and set no. of threads to that value - DONE
 * 2. command line options handler - DONE
 * 3. Unix socket listener for registering/unregistering jobs; probably good if separate thread
 * 4. Overrun mitigation?
 */


int main(int argc, char *argv[])
{

    int cli_index = 0, cliopt;
    std::list<class Job> regjobs, unregjobs;

    main_quit = false;
    static struct option cli[] = {
        {"register",    required_argument,  NULL,       'r'},
        {"unregister",  required_argument,  NULL,       'u'},
        {"id",          required_argument,  NULL,       'i'},
        {"version",     no_argument,        NULL,       'v'},
        {"listjobs",    no_argument,        NULL,       'l'},
        {NULL,          0,                  NULL,       0}
    };

    while((cliopt = getopt_long(argc, argv, ":r:u:i:l", cli, &cli_index)) != -1) {
        switch(cliopt) {
            case ':':
                std::cout<<"Invalid or missing arguments"<<std::endl;
            case 0:
                std::cout<<"Usage: "<<argv[0]<<" [-r \"job, interval\" -u \"job, interval\"] "<<std::endl<<
                           "\t\t -r \"job, interval\": registers a <job> that is to be repeated at every <interval> intervals"<<std::endl<<
                           "\t\t -u \"job, interval\": unregisters a previously registered <job>"<<std::endl;
                exit(0);
                break;
            case 'v':
                std::cout<<"Josch v1.0"<<std::endl;
                std::cout<<"Written by Nalin Kanwar."<<std::endl;
                exit(0);
                break;
            case 'r':
            {
                //temporary; we'll take last argument as job later on so it works like 'watch'
                std::string arg = optarg;
                auto const pos = arg.find_last_of(",");

                if(pos != std::string::npos) {
                    std::string jobcmd = arg.substr(0, pos);
                    int interval = std::stol(arg.substr(pos+1));

                    if(interval == 0) {
                        LOG<<"Invalid interval "<<interval<<std::endl;
                    }

                    LOG<<"Registering "<<jobcmd<<" with interval "<<std::endl;

                    class tlv_client t;
                    if(t.init(std::string(DEFAULT_FPATH)) == false) {
                        LOG<<"Failed to init tlv client"<<std::endl;
                        exit(-1);
                    }
                    if(t.sendcmd(TLV_REGISTER_JOB, jobcmd, interval) == false) {
                        LOG<<"Failed to send command to josch"<<std::endl;
                    }
                    LOG<<"Registered '"<<jobcmd<<"' successfully"<<std::endl;

                    exit(0);
                } else {
                    //@FIXME invalid argument?
                }
            }
                break;
            case 'u':
            {
                //temporary; we'll take last argument as job later on so it works like 'watch'
                std::string arg = optarg;
                auto const pos = arg.find_last_of(",");

                if(pos != std::string::npos) {
                    std::string jobcmd = arg.substr(0, pos);
                    int interval = std::stol(arg.substr(pos+1));

                    if(interval == 0) {
                        LOG<<"Invalid interval "<<interval<<std::endl;
                    }

                    LOG<<"Unregistering "<<jobcmd<<" with interval "<<std::endl;

                    class tlv_client t;
                    if(t.init(std::string(DEFAULT_FPATH)) == false) {
                        LOG<<"Failed to init tlv client"<<std::endl;
                        exit(-1);
                    }
                    if(t.sendcmd(TLV_UNREGISTER_JOB, jobcmd, interval) == false) {
                        LOG<<"Failed to send command to josch"<<std::endl;
                    }
                    LOG<<"Unregistered '"<<jobcmd<<"' successfully"<<std::endl;
                    exit(0);
                } else {
                    //@FIXME invalid argument?
                }
            }
                break;
            case 'l':
            {
                class tlv_client t;
                std::string s;
                if(t.init(std::string(DEFAULT_FPATH)) == false) {
                    LOG<<"Failed to init tlv client"<<std::endl;
                    exit(-1);
                }
                if(t.sendcmd(TLV_LIST_JOBS, s, 0) == false) {
                    LOG<<"Failed to send command to josch"<<std::endl;
                }
                exit(0);
            }
            default:
                break;
        }
    }

    /* set child handler to reap zombie childs */
    signal(SIGCHLD, proc_exit);
    signal(SIGINT, handle_quit);
    signal(SIGPIPE, SIG_IGN);

    unsigned nHWthreads = std::thread::hardware_concurrency();
    //LOG<<"HWTheads: "<<nHWthreads<<std::endl;
    try {

        class Josch js(nHWthreads ? nHWthreads: MINTHREADS);
        class client_handler ch(&js);
        ch.init(std::string(DEFAULT_FPATH));

        js.handle_jobs();
        LOG<<"Finished handling jobs"<<endl;

        /* Make sure our TLV client handler thread also terminates before main thread */
        ch.die();
        ch.join();
    } catch (std::exception &e) {
        LOG<<"Exception :"<<e.what()<<std::endl;
        main_quit = true;
    }

    LOG<<"Exiting main thread"<<endl;

    return 0;
}
