#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <vector>
#include <errno.h>
#include <string.h>
#include <cassert>
using namespace std;

pthread_barrier_t bar;
static pthread_mutex_t outputlock;

struct thread_info_t
{
    pthread_t thread_id;
    int num_threads;
    int sched_policies;
    int sched_priorities;
    double time_wait;
};

static double thr_clock(void)
{
    struct timespec t;
    assert(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t) == 0);
    return t.tv_sec + 1e-9 * t.tv_nsec;
}

vector<string> split(const string &str, const string &separator)
{
    vector<string> new_str;
    size_t start = 0, end;

    while ((end = str.find(separator, start)) != string::npos)
    {
        new_str.push_back(str.substr(start, end - start));
        start = end + separator.length();
    }
    new_str.push_back(str.substr(start));
    return new_str;
}

void *thread_func(void *arg)
{
    thread_info_t *threadParams = static_cast<thread_info_t *>(arg);

    /* 1. Wait until all threads are ready */
    pthread_barrier_wait(&bar);

    /* 2. Do the task */
    for (int i = 0; i < 3; i++)
    {
        pthread_mutex_lock(&outputlock);
        cout << "Thread " << threadParams->num_threads << " is starting" << endl;
        pthread_mutex_unlock(&outputlock);

        // Busy for time_wait seconds
        double start_time = thr_clock();
        while (thr_clock() - start_time < threadParams->time_wait)
        {
            // Busy wait loop
        }
    }

    pthread_exit(0);
}

void print_scheduler(void)
{
    int schedType = sched_getscheduler(getpid());

    switch (schedType)
    {
    case SCHED_FIFO:
        printf("SCHED_FIFO\n");
        break;
    case SCHED_OTHER:
        printf("SCHED_OTHER\n");
        break;
    default:
        printf("UNKNOWN\n");
    }
}

int main(int argc, char **argv)
{
    int numOfThread = 0;
    double waitTime = 0.0;
    vector<string> policy;
    vector<int> priorities;
    int cmd_opt = 0;

    /* 1. Parse program arguments */
    while ((cmd_opt = getopt(argc, argv, "n:t:s:p:")) != -1)
    {
        switch (cmd_opt)
        {
        case 'n':
            numOfThread = atoi(optarg);
            break;
        case 't':
            waitTime = atof(optarg);
            break;
        case 's':
            policy = split(optarg, ",");
            break;
        case 'p':
            for (const auto &p : split(optarg, ","))
            {
                priorities.push_back(stoi(p));
            }
            break;
        case '?':
            cerr << "Illegal option: " << static_cast<char>(optopt) << endl;
            return 1;
        default:
            cerr << "Unsupported option" << endl;
        }
    }

    /* check for non-option arguments */
    for (int index = optind; index < argc; index++)
        cerr << "Non-option argument " << argv[index] << endl;

    /* 2. Create <num_threads> worker threads */
    vector<thread_info_t> threadParams(numOfThread);
    vector<pthread_attr_t> sched_attr(numOfThread);
    vector<struct sched_param> sched_param(numOfThread);

    pthread_barrier_init(&bar, nullptr, numOfThread);

    for (int i = 0; i < numOfThread; i++)
    {
        threadParams[i].num_threads = i;
        threadParams[i].sched_policies = (policy[i] == "NORMAL") ? SCHED_OTHER : SCHED_FIFO;
        threadParams[i].sched_priorities = priorities[i];
        threadParams[i].time_wait = waitTime;
    }

    /* 3. Set CPU affinity */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    for (int i = 0; i < numOfThread; i++)
    {
        /* 4. Set the attributes to each thread */
        pthread_attr_init(&sched_attr[i]);
        pthread_attr_setinheritsched(&sched_attr[i], PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&sched_attr[i], threadParams[i].sched_policies);
        pthread_attr_setaffinity_np(&sched_attr[i], sizeof(cpu_set_t), &cpuset);

        sched_param[i].sched_priority = (threadParams[i].sched_policies == SCHED_OTHER) ? 
                                        sched_get_priority_max(SCHED_OTHER) : threadParams[i].sched_priorities;

        if (sched_setscheduler(getpid(), threadParams[i].sched_policies, &sched_param[i]) < 0)
        {
            cerr << "[ Set scheduler fail ]" << strerror(errno) << endl;
        }

        pthread_attr_setschedparam(&sched_attr[i], &sched_param[i]);

        pthread_create(&threadParams[i].thread_id, // Pointer to thread descriptor
                       &sched_attr[i],             // Use default attributes
                       thread_func,                // Thread function entry point
                       (void *)&(threadParams[i])  // Parameters to pass in
        );
    }

    /* 5. Start all threads at once */
    for (int i = 0; i < numOfThread; i++)
    {
        pthread_join(threadParams[i].thread_id, nullptr);
    }

    /* 6. Wait for all threads to finish  */
    pthread_barrier_destroy(&bar);
    return 0;
}
