#include <iostream>
#include <thread>

#include "job.h"
#include "josch.h"

using namespace std;

uint64_t JobId::jobid_counter = 0;

int main(int argc, char *argv[])
{
    std::string s = "ls -lt /home/";

    class Job j(s, 500);
    class Job j2("ls -lt /var/", 100);
    class Josch js;

    js.init();
    js.register_job(j);
    js.register_job(j2);

    js.handle_jobs();

    LOG<<"Exiting main thread"<<endl;

    return 0;
}
