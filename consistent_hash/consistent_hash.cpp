
#include <sstream>
#include <string.h>
#include "consistent_hash.h"
#include "murmur_hash_3.h"


ConsistentHash::ConsistentHash(int node_num/* = 0*/, int virtual_node_num/* = 100*/)
{
	node_num_ = 0;
	virtual_node_num_ = virtual_node_num;
	Initialize(node_num);
}

ConsistentHash::~ConsistentHash()
{
	server_nodes_.clear();
}


uint32_t ConsistentHash::GetHashedNodeId(const char* key, uint32_t len)
{
	uint32_t partition = GetMurmurHash3_32(key, len);
	std::map<uint32_t, uint32_t>::iterator it = server_nodes_.lower_bound(partition);//沿环的顺时针找到一个大于等于key的虚拟节点

	if (it == server_nodes_.end())//未找到
	{
		return server_nodes_.begin()->second;
	}
	return it->second;
}

uint32_t ConsistentHash::GetHashedNodeId(const char* key)
{
	return GetHashedNodeId(key, strlen(key));
}

uint32_t ConsistentHash::GetHashedNodeId(uint32_t id)
{
	std::stringstream key;
	key << "MASK-KEY-ID" << id;
	return GetHashedNodeId(key.str().c_str(), key.str().length());
}

void ConsistentHash::GetAllPhysicalNodeId(std::set<uint32_t>& out)
{
	for (std::set<uint32_t>::iterator iter = servers.begin(); iter != servers.end(); ++iter)
	{
		out.insert(*iter);
	}
}

uint32_t ConsistentHash::GetMinNodeId()
{
	if (!servers.empty())
	{
		return *(servers.begin());
	}
	return 0;
}

void ConsistentHash::AddNewNode(const uint32_t id)
{
	for (uint32_t j = 0; j < virtual_node_num_; ++j)
	{
		std::stringstream node_key;
		node_key << "SHARD-" << id << "-NODE-" << j;
		uint32_t partition = GetMurmurHash3_32(node_key.str().c_str(), strlen(node_key.str().c_str()));
		server_nodes_.insert(std::pair<uint32_t, uint32_t>(partition, id));
	}
	++node_num_;
	servers.insert(id);
}

void ConsistentHash::DeleteNode(const uint32_t id)
{
	for (uint32_t j = 0; j < virtual_node_num_; ++j)
	{
		std::stringstream node_key;
		node_key << "SHARD-" << id << "-NODE-" << j;
		uint32_t partition = GetMurmurHash3_32(node_key.str().c_str(), strlen(node_key.str().c_str()));
		std::map<uint32_t, uint32_t>::iterator it = server_nodes_.find(partition);
		if (it != server_nodes_.end())
		{
			server_nodes_.erase(it);
		}
	}
	--node_num_;
	servers.erase(id);
}

void ConsistentHash::Initialize(uint32_t cnt)
{
	for (uint32_t i = 0; i < cnt; ++i)
	{
		AddNewNode(i);
	}
}
