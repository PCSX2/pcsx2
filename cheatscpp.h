#ifndef CHEATSCPP_H_INCLUDED
#define CHEATSCPP_H_INCLUDED

class Group 
{
public:
	string title;
	bool enabled;
	int  parentIndex;

	Group(int nParent,bool nEnabled, string &nTitle);

};

class Patch
{
public:
	string title;
	int  group;
	bool enabled;
	int  patchIndex;

	Patch(int patch, int grp, bool en, string &ttl);

	Patch operator =(const Patch&p);
};

extern vector<Group> groups;
extern vector<Patch> patches;

#endif//CHEATSCPP_H_INCLUDED
