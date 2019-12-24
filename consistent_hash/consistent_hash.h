

#pragma once

#include <map>
#include <set>
#include <stdint.h>

class ConsistentHash
{
public:
	ConsistentHash(int node_num = 0, int virtual_node_num = 100);
	~ConsistentHash();

	//获取输入内容一致性哈希后对应的node的id
	uint32_t GetHashedNodeId(const char* key, uint32_t len);
	uint32_t GetHashedNodeId(const char* key);
	uint32_t GetHashedNodeId(uint32_t id);
	
	//获取所有真实节点
	void GetAllPhysicalNodeId(std::set<uint32_t>& out);

	//获取id最小的节点
	uint32_t GetMinNodeId();

	void AddNewNode(const uint32_t id);
	void DeleteNode(const uint32_t id);

private:
	void Initialize(uint32_t cnt);

private:
	std::map<uint32_t, uint32_t> server_nodes_; //虚拟节点,key是哈希值，value是节点的id
	std::set<uint32_t> servers;	//真实机器节点
	uint32_t node_num_;//真实机器节点个数
	uint32_t virtual_node_num_;//每个机器节点关联的虚拟节点个数
};