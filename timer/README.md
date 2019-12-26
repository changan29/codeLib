### 概述

一个Timer的实现需要具备以下几个行为:

- StartTimer(Interval, ExpiryAction)

注册一个时间间隔为 Interval 后执行 ExpiryAction 的定时器实例，其中，返回 TimerId 以区分在定时器系统中的其他定时器实例。

- StopTimer(TimerId)

根据 TimerId 找到注册的定时器实例并执行 Stop 。

- PerTickBookkeeping()

在一个 Tick 时间粒度内，定时器系统需要执行的动作，它最主要的行为，就是检查定时器系统中，是否有定时器实例已经到期。


具体的代码实现思路就是：在StartTimer的时候，把 当前时间 + Interval 作为key放入一个容器，然后在Loop的每次Tick里，从容器里面选出一个最小的key与当前时间比较，如果key小于当前时间，则这个key代表的timer就是expired，需要执行它的ExpiryAction(一般为回调)。

### 链表的实现
- 精度是 1ms
- 最长时间是10min,延长时间可以增加 slot数量,slot时间的间隔是 1ms
- 通过继承Timer父类，在子类重写timeout实现 超时回调
- 每次都需要遍历超过时间的所有链表，时间复杂度为O(n)

### 执行方式
每次从上次执行的时间，遍历每个链表上挂的timer是不是到期，如果到期了，就执行对应的超时函数，并移除定时器，把这个环拉直看就可以了:
![image](https://201910-1251969284.cos.ap-shanghai.myqcloud.com/time_round_1.jpg)
```
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

					ERROR("node == NULL");
					continue;
				}
				if (node->t == NULL)
				{
					ERROR("timer == NULL");
					continue;
				}
				DEBUG("time out %d", index);
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
```


### example
子类:
```
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
```

等待一段时间:

```
    ...
    unsigned wait = NumberHelper::ARandRange(6, 60000);
    BaseRobot* robot = new BaseRobot;
    robot->RobotWait(1234);
    ...
```

