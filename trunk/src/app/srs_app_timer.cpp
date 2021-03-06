/*
The MIT License (MIT)

Copyright (c) 2013-2014 winlin & Baidu Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <srs_app_timer.hpp>

#include <srs_kernel_error.hpp>
#include <srs_app_server.hpp>

SrsTimer::SrsTimer(int _interval) {
    timer_id = -1;
    paused = false;
    click_cnt = interval = _interval;
}

SrsTimer::~SrsTimer() {}

void SrsTimer::set_timer_id(int _timer_id) {
    timer_id = _timer_id;
}

int SrsTimer::get_timer_id() {
    return timer_id;
}

int SrsTimer::click() {
    --click_cnt;
    if (click_cnt == 0) {
        click_cnt = interval;
        return 0;
    } else {
        return click_cnt;
    }
}

void SrsTimer::set_interval(int _interval) {
    click_cnt = interval = _interval;
}

void SrsTimer::pause() {
    paused = true;
}

void SrsTimer::resume() {
    paused = false;
}

bool SrsTimer::is_paused() {
    return paused;
}

SrsTimerMgr::SrsTimerMgr(SrsServer* srs_server) {
    timer_id_alloc = 0;
    server = srs_server;

    pthread = new SrsThread(this, 1*1000*1000LL, true);
}

SrsTimerMgr::~SrsTimerMgr() {
    srs_freep(pthread);
}

int SrsTimerMgr::regist_timer(SrsTimer* timer) {
    int ret = ERROR_SUCCESS;
    if (timer->get_timer_id() >= 0) {
        // TODO: print an error log
        ret = ERROR_TIMER_EXIST;
        return ret;
    }

    // allocate an id
    while (true) {
        if (timers.find(timer_id_alloc) == timers.end()) {
            timer->set_timer_id(timer_id_alloc);
            timers[timer_id_alloc] = timer;
            timer_id_alloc = (timer_id_alloc+1) % TIMER_ID_ALLOC_MASK;
            break;
        }
        timer_id_alloc = (timer_id_alloc+1) % TIMER_ID_ALLOC_MASK;
    }

    return ret;
}

int SrsTimerMgr::remove_timer(SrsTimer* timer) {
    int ret = ERROR_SUCCESS;

    int timer_id = timer->get_timer_id();
    if (timer_id < 0) {
        return ret;
    }

    timers.erase(timer_id);
    timer->set_timer_id(-1);

    return ret;
}

int SrsTimerMgr::start() {
    return pthread->start();
}

int SrsTimerMgr::cycle() {
    int ret = ERROR_SUCCESS;

    for (std::map<int, SrsTimer*>::iterator itimer = timers.begin(); itimer != timers.end(); itimer++) {
        SrsTimer* timer = itimer->second;
        if (!timer->is_paused() && (timer->click()) == 0) {
            timer->callback();
        }
    }

    return ret;
}
