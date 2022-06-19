//
// Created by Peter Liu on 2022/6/19 0019.
//

#ifndef WEPLAYKT_SAFEQUEUE_H
#define WEPLAYKT_SAFEQUEUE_H

#include <queue>
#include <pthread.h>

using namespace std;

template<typename T> // 泛型：存放任意类型

class SafeQueue {
private:
    //void (*releaseCallback)(T *) const
    typedef void (*ReleaseCallback)(T *);

public:
    queue<T> queue;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int work;// 标记队列是否工作
    ReleaseCallback releaseCallback;

    SafeQueue() {
        pthread_mutex_init(&mutex, 0);
        pthread_cond_init(&cond, 0);
    }

    ~SafeQueue() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }

    /**
    * 入队 [ AVPacket *  压缩包]  [ AVFrame * 原始包]
    */
    void insertToQueue(T value) {
        pthread_mutex_lock(&mutex);
        if (work) {
            queue.push(value);
            pthread_cond_signal(&cond);
        } else {
            if (releaseCallback) {
                releaseCallback(&value);
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    // get T  如果没有数据，我就睡觉
    /**
    *  出队 [ AVPacket *  压缩包]  [ AVFrame * 原始包]
    */
    int getQueueAndDel(T &value) {
        int ret = 0;
        pthread_mutex_lock(&mutex);
        while (work && queue.empty()) {
            pthread_cond_wait(&cond, &mutex);
        }
        if (!queue.empty()) { //work and queue not empty
            value = queue.front(); //获取队列头部的元素
            queue.pop(); // 删除队列中头部的数据
            ret = 1;
        }
        pthread_mutex_unlock(&mutex);
        return ret;
    }

    /**
    * 设置工作状态，设置队列是否工作
    * @param work
    */
    void setWork(int work) {
        pthread_mutex_lock(&mutex);
        this->work = work;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }

    int empty() {
        return queue.empty();
    }

    int size() {
        return queue.size();
    }

    /**
     * 清空队列中所有的数据，循环一个一个的删除
     */
    void clear() {
        pthread_mutex_lock(&mutex);
        unsigned int size = queue.size();
        for (int i = 0; i < size; ++i) {
            T value = queue.front();
            if (releaseCallback) {
                releaseCallback(&value);// 让外界释放我们的 value
            }
            queue.pop();// 删除队列中的数据
        }
        pthread_mutex_unlock(&mutex);
    }

    void setReleaseCallback(ReleaseCallback releaseCallback) {
        this->releaseCallback = releaseCallback;
    }
};

#endif //WEPLAYKT_SAFEQUEUE_H
