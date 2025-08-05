#ifndef TIMER_H
#define TIMER_H

#include <time.h>

class Time : public timespec
{
	void normalize();
public:
	Time();
	double elapsed(Time const &t);
	void add(double d);
	double get();
	void set(double d);
};

class Timer
{
public:
	double m_duration;
	Timer *m_next;
	bool active;

	Timer(double t = 0);
	void set(double t);
	virtual void timeout() {}
};

class TimerList
{
public:
	Time m_ref;
	Timer m_first;

 	TimerList();

	void refresh();
	double read(Timer *timer);
	void add(Timer *newtimer);
	void del(Timer *timer);
	Time* check(Time *t);
};

#endif
