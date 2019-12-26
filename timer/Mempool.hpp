#ifndef COMMON_MEMPOOL_HPP_
#define COMMON_MEMPOOL_HPP_
#include <queue>
#include <cstdlib>
#include <assert.h>
class Mempool
{
public:
	void Init(uint64_t nodeSize)
	{
		m_nodeSize = nodeSize;
	}
	char* Alloc()
	{
		char* p = NULL;
		if (pool.size() == 0)
		{
			 p = new char[m_nodeSize];
			 assert(p != NULL);
		}
		else
		{
			p = pool.front();
			pool.pop();
		}

		return p;
	}
	void Free(char* p)
	{
		if (p != NULL)
		{
			pool.push(p);
		}
	}

private:
	std::queue<char*> pool;
	uint32_t m_nodeSize;
};


#endif /* COMMON_MEMPOOL_HPP_ */
