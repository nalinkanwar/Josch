#ifndef JOB_H
#define JOB_H

#include <thread>
#include <iostream>
#include <chrono>
#include <string>

extern std::atomic<int> log_level;

#define LOG std::cout<<"["<<std::this_thread::get_id()<<"] "\

/* This class is used to give unique id to new Jobs;
 * We just use a constantly increasing static unsigned integer
 * which will roll over on overflow */
class JobId {
    private:
        static uint64_t jobid_counter;
    protected:
        JobId() {

        }

        int64_t nextJobId() {
            return jobid_counter++;
        }

    public:
        uint64_t static get_curr_job_id() {
            return jobid_counter;
        }

        void static printCurrJobId() {
            LOG<<"jobid : "<<jobid_counter<<std::endl;
        }
};


/* This is the actual Job class that will store individual jobs scheduled.
 * we'll need to store the actual command to trigger and it's arguments.
 *
 * jobid: Unique ID for this Job; this is set in ctor when actual jobid is incremented
 * lastrun: stores the last successful run of the job; used to calculate
 *          when the job is to be scheduled next;
 * interval: The time period after which the job should be re-run.
 */
class Job : public JobId {
    private:
        uint64_t jobid;
        int64_t overruns = 0;        
        std::chrono::duration<int, std::milli> interval;

        std::string command;
        std::chrono::time_point<std::chrono::steady_clock> lastrun;
        bool scheduled = false;
    public:
        Job();
        Job(const std::string cmd, int ival);
        Job(class Job& j);
        Job(const class Job& j);
        ~Job();

        uint64_t get_job_id() const;
        const char *get_command() const;
        int64_t get_interval() const;
        int64_t get_overruns();

        void set_scheduled();
        void reset_overruns();

        bool operator<(const class Job& jright);
        bool spawn_process() const;

        bool next_run(); /* Whether Job is ready for next run or not */
};

class JobCompare {
    public:
        bool operator() (class Job& jleft, class Job& jright) {
            return (jleft < jright);
        }
};


#endif // JOB_H

