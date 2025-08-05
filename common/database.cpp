#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include "database.h"
#include "palma.h"

#define MIN(a,b) (a < b ? a : b)
#define MAX(a,b) (a > b ? a : b)

AssignableSet::AssignableSet(uint64_t addr, uint64_t count) :
										AddrSet(addr, count),
										m_next_free(NULL),
										m_ptr(NULL) {}

AssignableSet::AssignableSet(AddrSet* set) : AddrSet(set->getFirstAddr(), set->getSize()),
											m_next_free(NULL),
											m_ptr(NULL){}
	
void AssignableSet::chain(AssignableSet *set)
{
	if(set == NULL)
	{
		set = this;
		m_next_free = this;
		m_ptr = this;
	}
	else
	{
		m_next_free = set;
		AssignableSet *prev = (AssignableSet*) set->m_ptr;
		prev->m_next_free = this;
		m_ptr = prev;
		set->m_ptr = this;
	}
}

bool AssignableSet::unchain(void *db)
{
	bool last_set = false;
	if(m_next_free != this)
	{
		((AssignableSet*) m_ptr)->m_next_free = m_next_free;
		m_next_free->m_ptr = m_ptr;
	}
	else
		last_set = true;
	m_next_free = NULL;
	m_ptr = db;
	return last_set;	
}

void AssignableSet::timeout()
{
	SetDatabase *db = (SetDatabase*) m_ptr;
	AssignableSet *prev_set = db->search(getFirstAddr() - 1);
	AssignableSet *next_set = db->search(getLastAddr() + 1);
	chain(db->m_free_list);
	if(db->m_free_list == NULL)
		db->m_free_list = this;
	if(next_set != NULL && next_set->m_next_free != NULL)
		db->joinAndDelete(this);
	if(prev_set != NULL && prev_set->m_next_free != NULL)
		db->joinAndDelete(prev_set);	
}

TreeNode::TreeNode()
{
	m_parent = NULL;
	m_child[0] = NULL;
	m_child[1] = NULL;
	m_child[2] = NULL;
	m_set[0] = NULL;
	m_set[1] = NULL;
}


TreeNode::~TreeNode()
{
	delete(m_child[0]);
	delete(m_child[1]);
	delete(m_child[2]);
	delete(m_set[0]);
	delete(m_set[1]);
}

TreeNode *TreeNode::locate(uint64_t addr, int &index)
{
	if(addr >= m_set[0]->getFirstAddr() && addr <= m_set[0]->getLastAddr())
	{
		index = 0;
		return this;
	}
	else if(m_set[1] != NULL && addr >= m_set[1]->getFirstAddr() && addr <= m_set[1]->getLastAddr())
	{
		index = 1;
		return this;
	}
	else if(m_child[0] == NULL)
	{
		index = -1;
		return this;
	}
	if(addr < m_set[0]->getFirstAddr())
		return m_child[0]->locate(addr, index);
	if(m_set[1] == NULL || addr > m_set[1]->getLastAddr())
		return m_child[2]->locate(addr, index);
	return m_child[1]->locate(addr, index);	
}

AssignableSet *TreeNode::getSet(int index)
{
	if(index >= 0)
		return m_set[index];
	return NULL;
}

void TreeNode::setChild(int index, TreeNode *&item)
{
	m_child[index] = item;
	if(item != NULL)
		item->m_parent = this;
	item = NULL;
}

void TreeNode::add(AssignableSet *set, TreeNode *item)
{
	AssignableSet *r = set;
	if(m_set[1] == NULL)
	{
		if(set->getFirstAddr() < m_set[0]->getFirstAddr())
		{
			set = m_set[0];
			m_set[0] = r;
			setChild(1, item);
		}
		else
		{
			m_child[1] = m_child[2];
			setChild(2, item);
		}
		m_set[1] = set;
		return;
	}
	TreeNode *sibling = new TreeNode();
	if(set->getFirstAddr() < m_set[0]->getFirstAddr())
	{
		set = m_set[0];
		m_set[0] = r;
		sibling->m_set[0] = m_set[1];
		sibling->setChild(0, m_child[1]);
		sibling->setChild(2, m_child[2]);
		setChild(2, item);			
	}
	else
	{ 
		if(set->getFirstAddr() > m_set[1]->getFirstAddr())
		{
			sibling->m_set[0] = set;
			set = m_set[1];
			sibling->setChild(0, m_child[2]);
			sibling->setChild(2, item);
		}
		else
		{
			sibling->m_set[0] = m_set[1];
			sibling->setChild(0, item);
			sibling->setChild(2, m_child[2]);
		}
		m_child[2] = m_child[1];
		if(item != NULL)
			item->m_parent = sibling;
	}
	m_child[1] = NULL;
	m_set[1] = NULL;	
	if(m_parent != NULL)
	{
		m_parent->add(set, sibling);
	}
	else
	{
		TreeNode *parent = new TreeNode();
		parent->m_set[0] = set;
		parent->m_child[0] = this;
		parent->m_child[2] = sibling;
		m_parent = sibling->m_parent = parent;
	}	
}

int TreeNode::childIndex()
{
	if(!m_parent)
		return -1;
	for(int i=0; i<=2; i++)
	{
		if(m_parent->m_child[i] == this)
			return i;
	}
	return -1;
}

int TreeNode::redistribute_left(int index)
{
	if(index <= 0)
		return 0;
	int sib_idx = (m_parent->m_child[1] != NULL) ? index-1 : index-2;
	TreeNode *sibling = m_parent->m_child[sib_idx];
	int parent_ridx = (m_parent->m_child[1] != NULL) ? index-1 : 0;
	if(sibling->m_set[1] != NULL)
	{
		m_set[0] = m_parent->m_set[parent_ridx];
		m_parent->m_set[parent_ridx] = sibling->m_set[1];
		sibling->m_set[1] = NULL;
		m_child[2] = m_child[0];
		setChild(0, sibling->m_child[2]);
		sibling->setChild(2, sibling->m_child[1]);
		return 1;
	}
	return 0;
}

int TreeNode::redistribute_right(int index)
{
	if(index >= 2)
		return 0;
	int sib_idx = (m_parent->m_child[1] != NULL) ? index+1 : index+2;
	TreeNode *sibling = m_parent->m_child[sib_idx];
	if(sibling->m_set[1] != NULL)
	{
		int parent_ridx = (m_parent->m_child[1] != NULL) ? index : 0;
		m_set[0] = m_parent->m_set[parent_ridx];
		m_parent->m_set[parent_ridx] = sibling->m_set[0];
		sibling->m_set[0] = sibling->m_set[1];
		sibling->m_set[1] = NULL;
		setChild(2, sibling->m_child[0]);
		sibling->setChild(0, sibling->m_child[1]);
		return 1;
	}
	return 0;
}

int TreeNode::redistribute_up(int index)
{
	if(m_parent->m_set[1] != NULL)
	{
		switch(index)
		{
			case 0:
				m_set[0] = m_parent->m_set[0];
				m_set[1] = m_parent->m_child[1]->m_set[0];
				m_parent->m_set[0] = m_parent->m_set[1];
				setChild(1, m_parent->m_child[1]->m_child[0]);
				setChild(2, m_parent->m_child[1]->m_child[2]);
				break;
			case 1:
				m_parent->m_child[0]->m_set[1] = m_parent->m_set[0];
				m_parent->m_set[0] = m_parent->m_set[1];
				m_parent->m_child[0]->setChild(1, m_parent->m_child[0]->m_child[2]);
				m_parent->m_child[0]->setChild(2, m_parent->m_child[1]->m_child[0]);
				break;
			case 2:
				m_set[0] = m_parent->m_child[1]->m_set[0];
				m_set[1] = m_parent->m_set[1];
				setChild(2, m_child[0]);
				setChild(1, m_parent->m_child[1]->m_child[2]);
				setChild(0, m_parent->m_child[1]->m_child[0]);
				break;
		}
		m_parent->m_child[1]->m_set[0] = NULL;
		m_parent->m_child[1]->m_child[0] = m_parent->m_child[1]->m_child[2] = NULL;
		m_parent->m_set[1] = NULL;
		TreeNode *item = m_parent->m_child[1];
		m_parent->m_child[1] = NULL;
		delete item;
		return 1;
	}
	return 0;
}

TreeNode *TreeNode::merge(int index)
{
	TreeNode *parent = m_parent;
	if(index == 0)
	{
		m_set[0] = parent->m_set[0];
		m_set[1] = parent->m_child[2]->m_set[0];
		parent->m_child[2]->m_set[0] = NULL;
		setChild(1, parent->m_child[2]->m_child[0]);
		setChild(2, parent->m_child[2]->m_child[2]);
	}
	else
	{
		m_parent->m_child[0]->m_set[1] = m_parent->m_set[0];
		parent->m_child[0]->setChild(1, parent->m_child[0]->m_child[2]);
		parent->m_child[0]->setChild(2, m_child[0]);
		m_set[0] = NULL;
	}
	delete parent->m_child[2];
	parent->m_set[0] = NULL;
	parent->m_child[2] = NULL;
	if(parent->m_parent != NULL)
		return(parent->redistribute());
	TreeNode *new_root = parent->m_child[0];
	new_root->m_parent = NULL;
	parent->m_child[0] = NULL;
	delete parent;
	return new_root;	
}

TreeNode *TreeNode::redistribute()
{
	int child_index = childIndex();
	if(redistribute_left(child_index) || 
		redistribute_right(child_index) ||
		redistribute_up(child_index))
		return NULL;
	return merge(child_index);
}

TreeNode *TreeNode::del(int index)
{
	AssignableSet *set = m_set[index];
	TreeNode *new_root = NULL;
	if(m_child[0] != NULL) //search inorder successor
	{
		int idx;
		TreeNode *successor = locate(m_set[index]->getLastAddr()+1, idx);
		m_set[index] = successor->m_set[0];
		successor->m_set[0] = set;
		return(successor->del(0));
	}
	else if(m_set[1] != NULL)
	{
		m_set[index] = m_set[1-index];
		m_set[1] = NULL;
	}
	else
	{
		new_root = redistribute();
		
	}
	delete set;
	return new_root; 
}


SetDatabase::SetDatabase(Palma *protocol) : m_protocol(protocol),
													m_total_set(){}

void SetDatabase::init(AddrSet *set) 
{
	m_total_set = *set;
	m_root = new TreeNode();
	m_root->m_set[0] = new AssignableSet(set);
	m_root->m_set[0]->m_next_free = m_root->m_set[0];
	m_root->m_set[0]->m_ptr = m_root->m_set[0];
	m_free_list = m_root->m_set[0];
}

SetDatabase::~SetDatabase()
{
	delete(m_root);
}

AssignableSet *SetDatabase::search(uint64_t addr)
{
	int index;
	return m_root->locate(addr, index)->getSet(index);
}

AssignableSet* SetDatabase::splitAndInsert(AssignableSet* set, uint64_t size)
{
	if(set->getSize() <= size)
		return NULL;
	AssignableSet *new_set = 
			new AssignableSet(set->getFirstAddr() + set->getSize() - size, size);
	set->setSize(set->getSize() - size);
	new_set->chain(set);
	int idx;
	m_root->locate(new_set->getFirstAddr(), idx)->add(new_set, NULL);
	if(m_root->m_parent)
		m_root = m_root->m_parent;
	return new_set;
}

void SetDatabase::joinAndDelete(AssignableSet *set)
{
	int index;
	TreeNode *node = m_root->locate(set->getLastAddr() + 1, index);
	AssignableSet *next = node->getSet(index);
	if(next->m_next_free == NULL)
		m_protocol->m_event_loop.stopTimer(next);
	else
	{
		next->unchain(NULL);
		if(m_free_list == next)
			m_free_list = set;
	}
	
	set->setSize(set->getSize() + next->getSize());
	TreeNode *new_root = node->del(index);
	if(new_root != NULL)
		m_root = new_root;
}

int SetDatabase::exclude(AddrSet *recv_set, uint16_t lifetime)
{
	AddrSet set;
	if(set.checkConflict(&m_total_set, recv_set))
	{
		AssignableSet *r = search(set.getFirstAddr());
		if(r == NULL)
			return -1;
		if(r->m_next_free == NULL)
		{
			if(r->getFirstAddr() == set.getFirstAddr()
				&& r->getSize() == set.getSize())
			{
				if(fabs(m_protocol->m_event_loop.readTimer(r) - lifetime) > 1.)
				{
					m_protocol->m_event_loop.stopTimer(r);
					m_protocol->m_event_loop.startTimer(r, lifetime);
				}
				return 0;
			}
			else
			{
				m_protocol->m_event_loop.stopTimer(r);
				r->chain(m_free_list);
				if(m_free_list == NULL)
					m_free_list = r;
			}		
		}
		while(r->getLastAddr() < set.getLastAddr())
		{
			joinAndDelete(r);
		}
		extract(r, &set);
		m_protocol->m_event_loop.startTimer(r, lifetime);
		return 0;
	}
	return -1;
}

AssignableSet* SetDatabase::getFreeSet(uint64_t min, uint64_t max, bool random)
{
	AssignableSet *p = m_free_list;
	AssignableSet *best;
	uint64_t best_size = 0, size;
	if(p == NULL)
		return NULL;
	do
	{
		size = p->getSize() > 0xffff ? MAX(p->getAlignedSize(), 0xffff) : p->getSize();
		size = MIN(size, max);
		if(best_size < size
				|| (random && best_size == size && (lrand48() % 1)))
		{
			best = p;
			best_size = size;
		}
		p = p->m_next_free;
		if(p == m_free_list)
			break;
	} while(best_size < max || random);
	if(best_size >= min)
		return best;
	else
		return NULL;
}
/*
AssignableSet* SetDatabase::getFreeSet(uint64_t min, uint64_t max, bool random)
{
	AssignableSet *p = m_free_list;
	AssignableSet *best;
	uint64_t best_size = 0, size;
	if(p == NULL)
		return NULL;
	do
	{
		size = p->getSize() > 0xffff ? MAX(p->getAlignedSize(), 0xffff) : p->getSize();
		if(best_size < size)
		{
			best = p;
			best_size = size;
		}
		p = p->m_next_free;
		if(p == m_free_list)
			break;
	} while(best_size < max);
	if(best_size >= min)
		return best;
	else
		return NULL;
} */

void SetDatabase::extract(AssignableSet* &container_set, AddrSet *set)
{
	uint64_t size = container_set->getLastAddr() - set->getFirstAddr() + 1;
	if(set->getFirstAddr() > container_set->getFirstAddr())
	{
		container_set = splitAndInsert(container_set, size);
	}
	size -= set->getSize();
	if(size)
	{
		splitAndInsert(container_set, size);
	}
	if(m_free_list == container_set)
		m_free_list = m_free_list->m_next_free;
	if(container_set->unchain(this))
		m_free_list = NULL;
}

AssignableSet* SetDatabase::findSet(uint64_t count)
{
	AssignableSet *free_set = getFreeSet(1, count);
	if(free_set == NULL)
		return NULL;
	AddrSet set = *free_set;
	if(MIN(free_set->getSize(),count) > 0xffff)
		set.alignToMask(SetType::ADDR);
	if(set.getSize() > count)
		set.setSize(count);
	extract(free_set, &set);
	return free_set;
}

AssignableSet* SetDatabase::reserve(uint64_t count, uint64_t security_id, uint16_t lifetime)
{
	AssignableSet *free_set = findSet(count);
	if(free_set == NULL)
		return NULL;
	free_set->m_security_id = security_id;
	free_set->m_reserved = true;
	m_protocol->m_event_loop.startTimer(free_set, lifetime);
	return free_set;
}

AddrSet* SetDatabase::assign(AssignableSet *container_set, AddrSet *set, uint64_t security_id, uint16_t lifetime)
{
	if(container_set->m_next_free == NULL)
	{
		m_protocol->m_event_loop.stopTimer(container_set);
		container_set->chain(m_free_list);
		if(m_free_list == NULL)
			m_free_list = container_set;
	}
	extract(container_set, set);
	container_set->m_security_id = security_id;
	container_set->m_reserved = false;
	m_protocol->m_event_loop.startTimer(container_set, lifetime + 1);
	return container_set;
}

AddrSet* SetDatabase::assign(uint64_t count, uint64_t security_id, uint16_t lifetime)
{
	AssignableSet *free_set = findSet(count);
	free_set->m_security_id = security_id;
	free_set->m_reserved = false;
	m_protocol->m_event_loop.startTimer(free_set, lifetime + 1);
	return free_set;
}

void SetDatabase::release(AssignableSet *set)
{
	m_protocol->m_event_loop.stopTimer(set);
	set->timeout();
}	

DbStatus SetDatabase::checkStatus(AddrSet *set, uint64_t &security_id, 
									uint16_t &lifetime, bool &identical, AssignableSet *&result)
{
	result = search(set->getFirstAddr());
	if(result != NULL && result->getLastAddr() >= set->getLastAddr())
	{
		identical = (result->getSize() == set->getSize());
		if(result->m_next_free != NULL)
			return DbStatus::FREE;
		security_id = result->m_security_id;
		lifetime = m_protocol->m_event_loop.readTimer(result);
		if(result->m_reserved)
			return DbStatus::RESERVED;
		if(lifetime > 0)
			lifetime--;
		return DbStatus::ASSIGNED;
	}
	else
		return DbStatus::INVALID;
}
