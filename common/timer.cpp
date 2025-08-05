#include <stdio.h>
#include <stdlib.h>
#include "timer.h"
#define NSPERS	(1000000000UL)

Time::Time()
{
	clock_gettime(CLOCK_MONOTONIC, this);
}

void Time::normalize()
{
	time_t s = tv_nsec/NSPERS;

	if(s)
	{
		tv_sec += s;
		tv_nsec = tv_nsec%NSPERS;
	}
	if(tv_sec < 0 && tv_nsec > 0)
	{
		tv_sec++;
		tv_nsec -= NSPERS;
	}
	else if(tv_sec > 0 && tv_nsec < 0)
	{
		tv_sec--;
		tv_nsec += NSPERS;
	}
}

double Time::elapsed(Time const &t)
{
	return (tv_nsec - t.tv_nsec)*1e-9 + tv_sec - t.tv_sec;
}

void Time::add(double d)
{
	time_t sec = (time_t)d;
	tv_sec += sec;
	tv_nsec += (long)((d - sec) * 1e9);
	normalize();
}

double Time::get()
{
	return tv_sec + tv_nsec * 1e-9;
}

void Time::set(double d)
{
	time_t sec = (time_t)d;
	tv_sec = sec;
	tv_nsec = (long)((d - sec) * 1e9);
}

Timer::Timer(double t) : m_duration(t), m_next(NULL), active(false) {}

void Timer::set(double t)
{
	m_duration = t;
}

TimerList::TimerList() : m_first()
{
	refresh();
}

void TimerList::refresh()
{
	Time now;
	if(m_first.m_next != NULL)
		m_first.m_next->m_duration -= now.elapsed(m_ref);
	m_ref = now;
}

double TimerList::read(Timer *timer)
{
	if(!timer->active)
		return 0.;

	double time = 0.;
	refresh();

	for(Timer *p = m_first.m_next; p != timer && p != NULL; p = p->m_next)
	{
		time += p->m_duration;
	}
	time += timer->m_duration;
	return time;
}

void TimerList::add(Timer *newtimer)
{
	refresh();

	newtimer->active = true;
	for(Timer *p = &m_first;; p = p->m_next)
	{
		if(p->m_next == NULL)
		{
			p->m_next = newtimer;	
			newtimer->m_next = NULL;
			return;
		}
		else if(newtimer->m_duration < p->m_next->m_duration)
		{
			p->m_next->m_duration -= newtimer->m_duration;  
			newtimer->m_next = p->m_next;
			p->m_next = newtimer;
			return;	
		}
		else
			newtimer->m_duration -= p->m_next->m_duration;
	}
}

void TimerList::del(Timer *timer)
{
	Timer *p;
	double left = 0.;

	if(!timer->active)
		return;
	refresh();
	for(p = &m_first; p->m_next != timer; p = p->m_next)
	{
		if(p->m_next == NULL) return;
		left += p->m_next->m_duration;
	}
	if(timer->m_next != NULL)
		timer->m_next->m_duration += timer->m_duration;
	p->m_next = timer->m_next;
	timer->active = false;
	timer->m_duration += left;
}

Time* TimerList::check(Time *t)
{
	if(m_first.m_next == NULL)
		return NULL;
	refresh();

	while(m_first.m_next != NULL && m_first.m_next->m_duration <= 0)
	{
		Timer *p = m_first.m_next;
		p->active = false;
		if(p->m_next != NULL)
			p->m_next->m_duration += p->m_duration;
		m_first.m_next = m_first.m_next->m_next;
		p->timeout();
		refresh();
	}
	if(m_first.m_next != NULL)
		t->set(m_first.m_next->m_duration);
	return t;
}

