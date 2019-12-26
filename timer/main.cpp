#include <iostream>
#include "randHelper.h"
#include "Timer.h"
#include "TimerMgr.h"
#include "stdint.h"

using namespace std;

template<class T>
class RobotTimer:public Timer
{
        typedef void (T::*Method)();
        public:
        RobotTimer(T * p, Method m):method(m),pRobot(p){}
        virtual void Timeout();
        private:
        Method method;
        T* pRobot;

};

template<class T>
void RobotTimer<T>::Timeout()
{
	(pRobot->*method)();
}


class BaseRobot
{
public:
	BaseRobot()
	{
		m_id = 0;
	}
	void RobotWait(int millseconds);	
	void Resume()
	{
		cout<< "BaseRobot::Resume" << endl;
		
		TimerMgr::GetInst()->RemoveTimer(m_id);
		cout << "RemoveTimer"<< m_id << endl;
	}
	uint32_t m_id;	
};

void BaseRobot::RobotWait(int millseconds)
{
	RobotTimer<BaseRobot>* t =  reinterpret_cast<RobotTimer<BaseRobot>*>(TimerMgr::GetInst()->TimerPool.Alloc());
    new (t) RobotTimer<BaseRobot>(this, &BaseRobot::Resume);
    t->SetTimer(millseconds);
    uint32_t tid = t->getId();
    TimerMgr::GetInst()->Insert(t);	
	m_id = tid;
}

int  main()
{
	TimerMgr::GetInst()->Init();
	
	TimerMgr::GetInst()->TimeRun();
	long start;
    long timeuse;
	long timeMark = TimerMgr::GetInst()->Now();
	
	while(true)
	{
		int timer_count = 0;
        timer_count = TimerMgr::GetInst()->TimeRun();
		start = TimerMgr::GetInst()->Now();
		timeuse = start - timeMark;
		cout << "timeuse:" << timeuse << "start:" << start << endl;
				
		unsigned wait = NumberHelper::ARandRange(6, 60000);
		BaseRobot* robot = new BaseRobot;
		robot->RobotWait(1234);
		
		timeMark = start;
		// do some random
		for(int i = 0; i< 100 ; i++)
		{
			unsigned idc = NumberHelper::ARandRange(6, 60000);
			for(unsigned j = 0; j< idc;j++ )
			{
				unsigned k = NumberHelper::ARandRange(6, 600000);
			}	
		}
		
	}


	return 0;
}
