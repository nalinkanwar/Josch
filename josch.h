#ifndef JOSCH_H
#define JOSCH_H

#include <chrono>
#include <list>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "job.h"

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
    public:
        Josch();

        bool init();
        void thread_loop();

        bool register_job(class Job& newj);
        bool unregister_job(uint64_t tmpjobid);

        void list_jobs();
        void handle_jobs();

        bool spawnExtraThreads(int n);
};


#endif // JOSCH_H
