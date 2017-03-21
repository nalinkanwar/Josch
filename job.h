#ifndef JOB_H
#define JOB_H

#include <thread>
#include <iostream>
#include <chrono>
#include <string>

#define LOG std::cout<<"["<<std::this_thread::get_id()<<"] "

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
        uint64_t static getCurrJobId() {
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

        mutable std::string command;
        mutable std::chrono::time_point<std::chrono::steady_clock> lastrun;
        mutable bool scheduled = false;
    public:
        Job();
        Job(std::string cmd, int ival);
        Job(class Job& j);
        Job(const class Job& j);

        uint64_t getJobId() const;
        std::string& getCommand() const;
        int64_t getInterval() const;
        int64_t getOverruns();

        void setScheduled();
        void resetOverruns();

        bool operator<(const class Job& jright);
        bool spawnProcess() const;

        bool nextRun(); /* Whether Job is ready for next run or not */
};

class JobCompare {
    public:
        bool operator() (class Job& jleft, class Job& jright) {
            return (jleft < jright);
        }
};


#endif // JOB_H

