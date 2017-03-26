#ifndef JOSCH_H
#define JOSCH_H

#include <chrono>
#include <list>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>

#include "job.h"

#define MINTHREADS 4

//template <typename clock>
class Josch {
    private:
        typedef std::chrono::steady_clock clocktype;
        typedef clocktype clk;
        typedef std::chrono::duration<clk> dur;
        typedef std::chrono::time_point<clk, dur> tp;

        std::list<class Job> jlist;     /* used by Producer thread (main only for now?) */
        std::mutex mut_jl;

        std::priority_queue<Job, std::vector<Job>, JobCompare> workqueue; /* Shared resource */
        std::mutex mut_wq;
        std::condition_variable cv_wq;

        std::vector<std::thread> tpvec; /* Consumers threads - Job Runners */

        std::atomic<bool> quit;
    public:
        Josch();
        Josch(unsigned numthreads);
        ~Josch();

        void thread_loop(std::atomic<bool> &quit_flag);
        bool register_job(std::string &cmd, int ival);
        bool unregister_job(uint64_t tmpjobid);
        bool unregister_job(std::string &cmd, int ival);

        std::string list_jobs();
        void handle_jobs();

        bool spawn_extra_threads(unsigned n);

        void die();
};


#endif // JOSCH_H
