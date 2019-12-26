#include "TimerMgr.h"
#include <sys/time.h>
#include <stdio.h>

void TimerMgr::Init()
{
	timeLen = 1000 * 60 * 10;

	slot = new TimerList*[timeLen];
	for (int i = 0; i < timeLen; i++)
	{
		slot[i] = new TimerList();
	}

	//int size = TimerPool.GetAllocSize(timernum, sizeof(Timer) + 128);
	//char* timerBuff = new char[size];
	//TimerPool.Init(timerBuff, size, sizeof(Timer) + 128);
	TimerPool.Init(sizeof(Timer) + 128);

	// size = NodePool.GetAllocSize(timernum, sizeof(TimerNode));
	// char* nodeBuf = new char[size];
	// NodePool.Init(nodeBuf, size, sizeof(TimerNode));
	NodePool.Init(sizeof(TimerNode));
	Tick();
	lastRunTime = Now();



}





void TimerMgr::Insert(Timer* timer)
{
	TimerNode* node = reinterpret_cast<TimerNode*>(NodePool.Alloc());
	node->t = timer;
	Insert(node);

}
void TimerMgr::Insert(TimerNode* node)
{
	int index = node->t->t % timeLen;
	slot[index]->PushBack(node);
	printf("insert timer in %d, time len:%lu", index, node->t->t);
	nodemap.insert(std::make_pair(node->t->getId(), node));
}

void TimerMgr::RemoveTimer(int id)
{
	TimerNodeMap::iterator it;
	it = nodemap.find(id);
	if (it != nodemap.end())
	{
		nodemap.erase(id);
	}
}

int TimerMgr::TimeRun()
{
	Tick();
	int timeout_count = 0;
	int pass = currTime - lastRunTime;
	uint64_t time = lastRunTime + 1;

	for (int i = 0; i < pass; i++)
	{
		int index = time % timeLen;

		while(slot[index]->Size() > 0)
		{
			TimerNode* node = slot[index]->Serve();

			TimerNodeMap::iterator it;
			it = nodemap.find(node->t->getId());

			if (it != nodemap.end())
			{
				if (node == NULL)
				{

					printf("node == NULL");
					continue;
				}
				if (node->t == NULL)
				{
					printf("timer == NULL");
					continue;
				}
				printf("time out %d", index);
				node->t->Timeout();
				nodemap.erase(node->t->getId());
				timeout_count++;
			}

			TimerPool.Free(reinterpret_cast<char*>(node->t));
			NodePool.Free(reinterpret_cast<char*>(node));
		}
		time++;
	}
	lastRunTime = currTime;
	return timeout_count;

}

int TimerMgr::GenerateId()
{
	return ++nextid;
}


uint64_t TimerMgr::Now()
{
	return currTime;

}
TimerMgr::TimerMgr():nextid(0)
{
	Tick();
}

void TimerMgr::Tick()
{
	struct timeval tv;
    gettimeofday(&tv,NULL);
    currTime =  tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
