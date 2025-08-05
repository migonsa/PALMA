#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include "addrset.h"

#define MAX_NUM_ATTRIBUTES  (16)
#define MAX_ID_LENGTH (256)
#define MAX_MSG_LENGTH (256)
#define TO_ADDR(ptr) (*(uint64_t*)ptr)
#define TO_SIZE(ptr) (*(uint64_t*)ptr)
#define TO_UINT(ptr) (*(uint16_t*)ptr)
#define TO_ADDRSET_PTR(ptr) ((AddrSet*)ptr)
#define TO_BOOL(ptr) (*(bool*)ptr)
#define TO_STRING(ptr) ((uint8_t*)ptr)

class AttributeList
{
public:
	char m_name[MAX_NUM_ATTRIBUTES][MAX_ID_LENGTH];
	char m_value[MAX_NUM_ATTRIBUTES][MAX_ID_LENGTH];
	int m_length;

	bool getValue(int index, const char *name, uint64_t &val);
	bool getString(int index, const char *name, char *&val);
};


class ConfigElement
{
public:
	
	virtual bool  init(AttributeList *attr){}
	virtual void* get() {return NULL;}
	virtual void set(void *ptr) {}
	virtual ~ConfigElement(){}
};

class ConfigAddr : public ConfigElement
{
	uint64_t m_addr;
public:
	
	ConfigAddr(uint64_t addr);
	bool init(AttributeList *attr);
	void* get() {return &m_addr;}
	void set(void *addr) { m_addr = *(uint64_t *)addr; }
};

class ConfigSize : public ConfigElement
{
	uint64_t m_size;
public:
	
	ConfigSize(uint64_t size);
	bool init(AttributeList *attr);
	void* get() {return &m_size;}
	void set(void *size) { m_size = *(uint64_t *)size; }
};

class ConfigInt : public ConfigElement
{
	uint16_t m_val;
public:
	
	ConfigInt(uint16_t val);
	bool init(AttributeList *attr);
	void* get(){return &m_val;}
	void set(void *val) { m_val = *(uint16_t *)val; }
};

class ConfigSet : public ConfigElement
{
	AddrSet m_set;
	
public:

	ConfigSet(uint64_t addr, uint64_t count);
	bool init(AttributeList *attr);
	void* get(){return &m_set;}
	void set(void *set) { m_set = *(AddrSet *)set; }
};

class ConfigBool : public ConfigElement
{
	bool m_logic_val;
	
public:

	ConfigBool(bool logic_val);
	bool init(AttributeList *attr);
	void* get(){return &m_logic_val;}
	void set(void *logic_val) { m_logic_val = *(bool *)logic_val; }
};

class ConfigString : public ConfigElement
{
	char *m_str;
	
public:

	ConfigString(char *str);
	~ConfigString();
	bool init(AttributeList *attr);
	void* get(){return m_str;}
	void set(void *str);
};


class Config
{
public:
	ConfigElement **m_list;
	const char **m_array_tags;
	const char *m_root_tag;

	int m_num_line;	

	virtual bool check(const char *fname) = 0;
	virtual int getMaxItem() = 0;

	static bool toUint64(char *str, uint64_t &val);
	void skipBlank(FILE *fp);
	void skipUntil(FILE *fp, const char *pat);
	void error(const char *fname, const char *msg);
	void set(int item, void* ptr);
	void* get(int item);
	bool read(const char *fname);
};

#endif
