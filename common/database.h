#ifndef DATABASE_H
#define DATABASE_H
#include "addrset.h"
#include "timer.h"

class Palma;

enum class DbStatus
{
	FREE,
	RESERVED,
	ASSIGNED,
	INVALID
};

class AssignableSet : public AddrSet, public Timer
{
public:
	AssignableSet *m_next_free;
	void *m_ptr;
	uint64_t m_security_id;
	bool m_reserved;

	AssignableSet(uint64_t addr=0, uint64_t count=1);
	AssignableSet(AddrSet *set);
	void chain(AssignableSet *set);
	bool unchain(void *db);
	void timeout();	
};

class TreeNode
{
public:
	TreeNode *m_parent;
	TreeNode *m_child[3];
	AssignableSet *m_set[2];

	TreeNode();
	~TreeNode();
	TreeNode *locate(uint64_t addr, int &index);
	AssignableSet *getSet(int index);
	void setChild(int index, TreeNode *&item);
	void add(AssignableSet *set, TreeNode *item);
	int childIndex();
	int redistribute_left(int index);
	int redistribute_right(int index);
	int redistribute_up(int index);
	TreeNode *merge(int index);
	TreeNode *redistribute();
	TreeNode *del(int index);
};

class SetDatabase
{
public:
	Palma *m_protocol;
	TreeNode *m_root;
	AssignableSet *m_free_list;
	AddrSet m_total_set;

	SetDatabase(Palma *protocol);
	~SetDatabase();
	void init(AddrSet *set);
	AssignableSet *search(uint64_t addr);
	AssignableSet* splitAndInsert(AssignableSet *set, uint64_t size);
	void joinAndDelete(AssignableSet *set);
	int exclude(AddrSet *set, uint16_t lifetime);
	AssignableSet* getFreeSet(uint64_t min, uint64_t max, bool random = false);

	void extract(AssignableSet* &container_set, AddrSet *set);
	AssignableSet* findSet(uint64_t count);
	AssignableSet* reserve(uint64_t count, uint64_t security_id, uint16_t lifetime);
	AddrSet* assign(AssignableSet *container_set, AddrSet *set, uint64_t security_id, uint16_t lifetime);
	AddrSet* assign(uint64_t count, uint64_t security_id, uint16_t lifetime);
	void release(AssignableSet *set);
	DbStatus checkStatus(AddrSet *set, uint64_t &security_id, uint16_t &lifetime, bool &identical, AssignableSet *&result);	
};

#endif
