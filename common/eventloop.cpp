#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "eventloop.h"

bool EventLoop::m_finalize = false;
ExitHandler EventLoop::m_first_hnd;

EventLoop::EventLoop()
{
	FD_ZERO(&m_readfds);
	m_nfds = 0;
	m_first_src.m_next = NULL;

	sigset_t blockset;

	sigemptyset(&blockset);         /* Block SIGINT, SIGTERM */
	sigaddset(&blockset, SIGINT);
	sigaddset(&blockset, SIGTERM);

	sigprocmask(SIG_BLOCK, &blockset, NULL);

	struct sigaction sa;
	sa.sa_handler = doExit;        /* Establish signal handler */
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
}

void EventLoop::doExit(int signum)
{
	for(ExitHandler *h = m_first_hnd.m_next; h != NULL; h = h->m_next)
		h->onExit();
	m_finalize = true;
}

void EventLoop::regSource(EventSource *src)
{
	src->m_next = m_first_src.m_next;
	m_first_src.m_next = src;
	FD_SET(src->m_fd, &m_readfds);
	if(m_nfds < src->m_fd) m_nfds = src->m_fd;
}

void EventLoop::regHandler(ExitHandler *hnd)
{
	hnd->m_next = m_first_hnd.m_next;
	m_first_hnd.m_next = hnd;
}

void EventLoop::startTimer(Timer *newtimer, double t)
{
	if(t != 0.) newtimer->set(t);
	m_timerlist.add(newtimer);
}

void EventLoop::stopTimer(Timer *timer)
{
	m_timerlist.del(timer);
}

double EventLoop::readTimer(Timer *timer)
{
	return m_timerlist.read(timer);
}

void EventLoop::unregSource(EventSource *src)
{
	m_nfds = 0;
	for(EventSource *s = &m_first_src; s != NULL; s = s->m_next)
	{
		if(s->m_next == src)
		{
			s->m_next = s->m_next->m_next;
			FD_CLR(src->m_fd, &m_readfds);
		}
		else if(m_nfds < s->m_next->m_fd) m_nfds = s->m_next->m_fd;
	}
}

void EventLoop::unregHandler(ExitHandler *hnd)
{
	for(ExitHandler *h = &m_first_hnd; h != NULL; h = h->m_next)
		if(h->m_next == hnd)
		{
			h->m_next = h->m_next->m_next;
			break;
		}
}

void EventLoop::run()
{
	fd_set rdfds;
	int n;
	Time timeout;
	sigset_t emptyset;
	sigemptyset(&emptyset);
	while(!m_finalize)
	{ 
		rdfds = m_readfds;

		n = pselect(m_nfds+1, &rdfds, NULL, NULL, 	
				m_timerlist.check(&timeout), &emptyset);
		for(EventSource *s = m_first_src.m_next; s != NULL && n > 0; s = s->m_next)
		{
			if(FD_ISSET(s->m_fd, &rdfds))
			{
				s->onInput();
				n--;
			}
		}
	}
}

