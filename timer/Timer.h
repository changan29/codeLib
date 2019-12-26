#ifndef TIMER_H_
#define TIMER_H_
#include <iostream>
#include <time.h>
#include <sys/time.h>

using namespace std;
class Timer
{
public:
	Timer();
	virtual ~Timer();
	virtual void Timeout();
	void SetTimer(long time);
	bool IsTimeout(long now);
	bool operator>(const Timer& timer);
	bool operator<(const Timer& timer);
	bool operator==(const Timer& timer);
	bool operator>=(const Timer& timer);
	bool operator<=(const Timer& timer);

	int getId() const
	{
		return id;
	}
	long t;

protected:

	int id;
};

#endif /* TIMER_H_ */
