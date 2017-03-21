#include <iostream>
#include <thread>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#include <getopt.h>

#include "job.h"
#include "josch.h"

using namespace std;

uint64_t JobId::jobid_counter = 0;

/* reap the zombie childrens */
void proc_exit(int signal)
{
    int wstatus, ret;
    while(1) {
        ret = waitpid(0, &wstatus, WNOHANG);
        if(ret == 0 || ret == -1) {
            break;
        }
    }
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

    static struct option cli[] = {
        {"register",    required_argument,  NULL,       'r'},
        {"unregister",  required_argument,  NULL,       'u'},
        {"id",          required_argument,  NULL,       'i'},
        {"version",     no_argument,        NULL,       'v'},
        {"listjobs",   no_argument,        NULL,       'l'},
        {NULL,          0,                  NULL,       0}
    };

    while((cliopt = getopt_long(argc, argv, "r:u:i:", cli, &cli_index)) != -1) {
        switch(cliopt) {
            case 0:
                //@FIXME print usage
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

                    std::cout<<"Registering "<<jobcmd<<" with interval "<<interval<<std::endl;
                    regjobs.push_back(Job(jobcmd, interval));
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

                    std::cout<<"Unregistering "<<jobcmd<<" with interval "<<interval<<std::endl;
                    unregjobs.push_back(Job(jobcmd, interval));
                } else {
                    //@FIXME invalid argument?
                }
            }
                break;
            case 'l':
            {
                //@FIXME TLV client
                exit(0);
            }
            default:
                break;
        }
    }

    if(regjobs.empty() && unregjobs.empty()) {
        std::cout<<"No jobs scheduled; Terminating.."<<std::endl;
        exit(-1);
    }

    /* set child handler to reap zombie childs */
    signal(SIGCHLD, proc_exit);

    unsigned nHWthreads = std::thread::hardware_concurrency();
    //LOG<<"HWTheads: "<<nHWthreads<<std::endl;
    class Josch js(nHWthreads ? nHWthreads: MINTHREADS);

    for (auto& job: regjobs) {
        js.register_job(job);
    }

    for (auto& job: unregjobs) {
        js.unregister_job(job);
    }
    js.list_jobs();

    js.handle_jobs();

    LOG<<"Exiting main thread"<<endl;

    return 0;
}
