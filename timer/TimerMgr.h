#ifndef TIMERMGR_H_
#define TIMERMGR_H_

#include "Singleton.h"
#include "Timer.h"
#include <time.h>
#include <tr1/unordered_map>
#include <stdint.h>
#include "LinkList.h"
#include "Mempool.hpp"

class TimerMgr:public Singleton<TimerMgr>
{
public:
	typedef LinkList<Timer*> TimerList;
	typedef LinkList<Timer*>::Node TimerNode;
	typedef std::tr1::unordered_map<int, TimerNode*> TimerNodeMap;

public:
	TimerMgr();
	void Init();
	void Insert(Timer* timer);
	uint64_t Now();
	void RemoveTimer(int id);
	int GenerateId();
	int TimeRun();
	void Tick();

	template<class T>
	T* CreateTimer(T** t);

	Mempool TimerPool;

private:
	Mempool NodePool;

	uint64_t currTime;
	uint64_t lastRunTime;
	int timeLen;
	int nextid;
	void Insert(TimerNode* node);
	TimerList** slot;
	TimerNodeMap nodemap;
};

template<class T>
T* TimerMgr::CreateTimer(T** t)
{
	*t = reinterpret_cast<T*>(TimerPool.Alloc());
	return *t;
}


#endif /* TIMERMGR_H_ */
