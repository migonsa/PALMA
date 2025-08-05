#include <string.h>
#include <ctype.h>
#include "config.h"


bool AttributeList::getValue(int index, const char *name, uint64_t &val)
{
	if (!strcmp(m_name[index], name))
	{
		return Config::toUint64(m_value[index], val);
	}
	return false;
}

bool AttributeList::getString(int index, const char *name, char *&val)
{
	if (!strcmp(m_name[index], name))
	{
		val = m_value[index];
		return true;
	}
	return false;
}

ConfigAddr::ConfigAddr(uint64_t addr) : m_addr(addr){}

bool ConfigAddr::init(AttributeList *attr)
{
	return(attr->m_length == 1 && attr->getValue(0, "addr", m_addr));
}

ConfigSize::ConfigSize(uint64_t size) : m_size(size) {}

bool ConfigSize::init(AttributeList *attr)
{
	return (attr->m_length == 1 && attr->getValue(0, "size", m_size));
}

ConfigInt::ConfigInt(uint16_t val) : m_val(val){}

bool ConfigInt::init(AttributeList *attr)
{
	uint64_t val;
	if(attr->m_length != 1 || !attr->getValue(0, "value", val))
		return false;
	m_val = val;
	return true;
}

ConfigSet::ConfigSet(uint64_t addr, uint64_t count) : m_set(addr, count){}

bool ConfigSet::init(AttributeList *attr)
{
	bool addr_ok = false;
	bool val_ok = false;
	uint64_t addr,val;
	SetType type;
	
	for(int i=0; i < attr->m_length; i++)
	{
		if (!addr_ok && attr->getValue(i, "addr", addr))
			addr_ok = true;
		else if (!val_ok && attr->getValue(i, "count", val))
		{
			type = SetType::ADDR;
			val_ok = true;
		}
		else if (!val_ok && attr->getValue(i, "mask", val))
		{
			type = SetType::MASK;
			val_ok = true;
		}
		else
			return false;
	}
	m_set = AddrSet(addr, val, SetSize::AUTO, type);
	return true;
}	

ConfigBool::ConfigBool(bool logic_val) : m_logic_val(logic_val){}

bool ConfigBool::init(AttributeList *attr)
{
	char *p;
	char *val;
	if(attr->m_length != 1 || !attr->getString(0, "value", val))
		return false;
	else
	{
		for(char *p = val; *p != 0; p++)
			*p = toupper(*p);
		if(!strcmp(val, "TRUE"))
			m_logic_val = 1;
		else if(!strcmp(val, "FALSE"))
			m_logic_val = 0;
		else
			return false;
	}
	return true;
}

ConfigString::ConfigString(char *str)
{
	m_str = NULL;
	if(str) 
		m_str = strdup(str);
}

bool ConfigString::init(AttributeList *attr)
{
	char* str;
	if(attr->m_length == 1 && attr->getString(0, "id", str))
	{
		delete(m_str);
		m_str = strdup(str);
	}
	else
		return false;
	return true;
}

void ConfigString::set(void *str)
{
	delete(m_str);
	m_str = NULL;
	if(str) 
		m_str = strdup((char *)str);
}

ConfigString::~ConfigString()
{
	delete m_str;
}

bool Config::toUint64(char *str, uint64_t &val)
{
	char *p;
	val = strtoull(str, &p, 0);
	if (p != &str[strlen(str)])
		return false;
	return true;
}

void Config::skipBlank(FILE *fp)
{
	const char *spaces = "\n\r \t";
	while(!feof(fp))
	{
		int c = fgetc(fp);
		const char *p = strchr(spaces,c);
		if(p == NULL)
		{
			ungetc(c,fp);
			break;
		}
		else if(p == spaces)
			m_num_line++;
	}
}

void Config::skipUntil(FILE *fp, const char *pat)
{
	int cnt=0;
	do
	{
		skipBlank(fp);
		fscanf(fp, pat,&cnt);
		if(!cnt)
			fgetc(fp);
	}
	while(!cnt && !feof(fp));
}

void Config::error(const char *fname, const char *msg)
{
	fprintf(stderr,"%s:%d: %s\n",fname, m_num_line, msg);
}

void Config::set(int item, void* ptr)
{
	m_list[item]->set(ptr);
}

void* Config::get(int item)
{
	return m_list[item]->get();
}

bool Config::read(const char *fname)
{
	FILE *fp = fopen(fname, "r");
	if(fp == NULL)
	{
		perror("Opening configuration file");
		return false;
	}
	char tag[MAX_ID_LENGTH];
	AttributeList attr;

	bool opened_tag = false;
	int root_state = -1;
	const char *last_tag = NULL;

	char error_msg[MAX_MSG_LENGTH];	
	m_num_line = 1;

	while(!feof(fp))
	{
		skipBlank(fp);
		if(!fscanf(fp, "<%255[^> \t\n\r]", tag))
		{
			if(root_state != 0)
				error(fname, "Invalid char: Expected '<'");
			else
				error(fname, "Extra chars: Expected EOF");
			return false;
		}
		if(tag[0] == '?')
			skipUntil(fp, "?>%n");
		else if(tag[0] == '!')
			skipUntil(fp, "-->%n");
		else if(tag[0] == '/')
		{
			if(opened_tag)
			{
				if(!strcmp(&tag[1],last_tag))
					opened_tag = false;
				else
				{
					sprintf(error_msg, "Expected element '%s' close.", last_tag);
					error(fname, error_msg);
					return false;
				}
			}
			else if(root_state == 1 && !strcmp(&tag[1],m_root_tag))
				root_state = 0;
			else
			{
				sprintf(error_msg, "Unexpected element '%s' close.", &tag[1]);
				error(fname, error_msg);
				return false;
			}
			skipUntil(fp, ">%n");
		}
		else if(root_state == -1)
		{
			if(!strcmp(tag,m_root_tag))
			{
				root_state = 1;
				skipUntil(fp, ">%n");
			}
			else
			{
				sprintf(error_msg, "Unexpected root element '%s'", tag);
				error(fname, error_msg);
				return false;
			}
		}	
		else if(root_state == 1 && !opened_tag)
		{
			if(tag[strlen(tag)-1] == '/')
				tag[strlen(tag)-1] = 0;
			else
			{
				attr.m_length = 0;
				for(;;)
				{
					skipBlank(fp);
					if(!fscanf(fp, "%255[^>= \t\n\r]", attr.m_name[attr.m_length]))
					{
						opened_tag = true;
						skipUntil(fp, ">%n");
						break;
					}
					skipBlank(fp);
					if(attr.m_name[attr.m_length][0] == '/')
					{
						opened_tag = false;
						skipUntil(fp, ">%n");
						break;
					}
					else
					{
						skipBlank(fp);
						if(fgetc(fp) != '=')
						{
							error(fname, "Invalid char: Expected '='.");
							return false;
						}
						skipBlank(fp);
						if(!fscanf(fp, "\"%255[^>\"]\"", attr.m_value[attr.m_length++]))
						{
							error(fname, 
								"Invalid attribute value format:"
								" Expected quotation marks enclosed string.");
							return false;
						}
					}
				}
			}
			int i;
			for (i=0; i<getMaxItem() && strcmp(tag, m_array_tags[i]); i++);
			if (i < getMaxItem())
			{
				if(!m_list[i]->init(&attr))
				{
					sprintf(error_msg, "Invalid attributes for tag: '%s'.",tag);
					error(fname, error_msg);
					return false;
				}
				last_tag = m_array_tags[i];
			}
			else
			{
				sprintf(error_msg, "Unknown tag: '%s'.",tag);
				error(fname, error_msg);
				return false;
			}
		}
		else
		{
			error(fname, "Syntax error.");
			return false;
		}
		skipBlank(fp);
	}
	fclose(fp);
	if(root_state == 1)
	{
		error(fname, "Unexpected EOF.");
		return false;
	}
	else if(root_state == -1)
	{
		error(fname, "Empty configuration.");
		return false;
	}
	return check(fname);
}

