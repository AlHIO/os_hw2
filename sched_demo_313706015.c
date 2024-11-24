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
typedef struct
{
    pthread_t thread_id;
    int thread_num;
    int sched_policy;
    int sched_priority;
    double time_wait;
} thread_info_t;

static double thr_clock(void)
{
    struct timespec t;
    assert(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t) == 0);
    return 1e-9 * t.tv_nsec + t.tv_sec;
}

vector<string> split(string str, string seperator)
{
    vector<string> new_str;
    size_t pos = 0;
    while ((pos = str.find(seperator)) != string::npos)
    {
        string token = str.substr(0, pos); // store the substring
        new_str.push_back(token);
        str.erase(0, pos + seperator.length()); /* erase() function store the current positon and move to next token. */
    }
    new_str.push_back(str); // it print last token of the string.
    return new_str;
}

void *thread_func(void *arg)
{
    thread_info_t *threadParams = (thread_info_t *)arg;
    pthread_barrier_wait(&bar);
    for (int i = 0; i < 3; i++)
    {
        pthread_mutex_lock(&outputlock);
        {
            cout << "Thread " << threadParams->thread_num << " is starting" << endl;
            fflush(stdout);
            double sttime = thr_clock();
            while (1)
            {
                if (thr_clock() - sttime > 1 * threadParams->time_wait)
                    break;
            }
        }
        pthread_mutex_unlock(&outputlock);
    }
    pthread_exit(0);
}

int main(int argc, char **argv)
{
    int numOfThread = 0;
    float waitTime = 0.0f;
    vector<string> policy;
    vector<string> temp;
    vector<int> priorities;
    int cmd_opt = 0;

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
            policy = split(string(optarg), ",");
            break;
        case 'p':
            temp = split(string(optarg), ",");
            for (size_t i = 0; i < temp.size(); i++) // Changed int to size_t
            {
                priorities.push_back(stoi(temp[i]));
            }
            break;
        case '?':
            cerr << "Illegal option: " << isprint(optopt) << endl;
            return 1;
        default:
            cerr << "not support options" << endl;
        }
    }

    vector<thread_info_t> threadParams(numOfThread); // Changed to vector
    vector<pthread_attr_t> sched_attr(numOfThread);  // Changed to vector
    vector<struct sched_param> sched_param(numOfThread); // Changed to vector

    pthread_barrier_init(&bar, NULL, numOfThread);

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    for (int i = 0; i < numOfThread; i++)
    {
        threadParams[i].thread_num = i;
        if (policy[i] == "NORMAL")
        {
            threadParams[i].sched_policy = 0;
        }
        else
        {
            threadParams[i].sched_policy = 1;
        }
        threadParams[i].sched_priority = priorities[i];
        threadParams[i].time_wait = waitTime;

        pthread_attr_init(&sched_attr[i]);
        pthread_attr_setinheritsched(&sched_attr[i], PTHREAD_EXPLICIT_SCHED);
        if (threadParams[i].sched_policy == 0)
        {
            pthread_attr_setschedpolicy(&sched_attr[i], SCHED_OTHER);
        }
        else
        {
            pthread_attr_setschedpolicy(&sched_attr[i], SCHED_FIFO);
        }

        pthread_attr_setaffinity_np(&sched_attr[i], sizeof(cpu_set_t), &cpuset);

        if (threadParams[i].sched_policy == 0)
        {
            sched_param[i].sched_priority = sched_get_priority_max(SCHED_OTHER);
            if (sched_setscheduler(getpid(), SCHED_OTHER, &sched_param[i]) < 0)
            {
                cerr << "[ Set scheduler fail - NORMAL]" << strerror(errno) << endl;
            }
        }
        else
        {
            sched_param[i].sched_priority = threadParams[i].sched_priority;
            if (sched_setscheduler(getpid(), SCHED_FIFO, &sched_param[i]) < 0)
            {
                cerr << "[ Set scheduler fail ]" << strerror(errno) << endl;
            }
        }

        pthread_attr_setschedparam(&sched_attr[i], &sched_param[i]);
        pthread_create(&threadParams[i].thread_id, &sched_attr[i], thread_func, (void *)&(threadParams[i]));
    }

    for (int i = 0; i < numOfThread; i++)
    {
        pthread_join(threadParams[i].thread_id, NULL);
    }

    pthread_barrier_destroy(&bar);
    return 0;
}
