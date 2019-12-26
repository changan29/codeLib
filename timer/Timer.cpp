#include "Timer.h"
#include "TimerMgr.h"


Timer::Timer():t(0),id(TimerMgr::GetInst()->GenerateId())
{
}

Timer::~Timer()
{

}


void Timer::SetTimer(long time)
{
	t = time + TimerMgr::GetInst()->Now();
}

bool Timer::operator >(const Timer& timer)
{
	return t > timer.t;
}

bool Timer::operator <(const Timer& timer)
{
	return t < timer.t;
}

bool Timer::operator ==(const Timer& timer)
{
	return t == timer.t;
}

bool Timer::operator >=(const Timer& timer)
{
	return t >= timer.t;
}

void Timer::Timeout()
{
	printf("base timeout");
}

bool Timer::operator <=(const Timer& timer)
{
	return t <= timer.t;
}

bool Timer::IsTimeout(long now)
{
	if (now >= t)
	{
		return true;
	}
	return false;
}
