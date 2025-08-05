#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include "timer.h"
#include <sys/select.h>

class EventSource
{
public:
	int m_fd;
	EventSource *m_next;

	virtual int onInput() {}
};

class ExitHandler
{
public:	
	ExitHandler *m_next = NULL;

	virtual void onExit() {}
};

class EventLoop
{
	fd_set m_readfds;
	int m_nfds;
	EventSource m_first_src;
	TimerList m_timerlist;

	static void doExit(int signum);

public:
	static ExitHandler m_first_hnd;
	static bool m_finalize;

	EventLoop();
	void regSource(EventSource *src);
	void regHandler(ExitHandler *hnd);
	void startTimer(Timer *newtimer, double t = 0.);
	void stopTimer(Timer *timer);
	double readTimer(Timer *timer);
	void unregSource(EventSource *src);
	void unregHandler(ExitHandler *hnd);
	void run();
};

#endif
