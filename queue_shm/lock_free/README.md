
## 特点
1. 健壮
2. 多读多写

## 例子
```cpp
#include "que_shm.h"

int main(int argc, char** argv)
{
	CQueShm que("test_shm", 10, 10, 10);
	
	std::string value;
	
	if(que.Put("value") != 0) return -1;
	
	if(que.Pop(value) != 0) return -1;
	
	return 0
}
```
value可以是变长、二进制数据。

## 接口
```cpp
/// @brief 构造函数
/// @param[in] sShmName 共享内存名字
/// @param[in] iQueSize 队列长度，可以容纳value的个数，只有在共享内存末创建时有效
/// @param[in] iValueBlockNum value block个数，一个value可以占用多个block，只有在共享内存末创建时有效
/// @param[in] iValueBlockDataSize value block大小，value内存分配的最小单位，只有在共享内存末创建时有效
CQueShm(const std::string& sShmName,
        int iQueSize,
        int iValueBlockNum,
        int iValueBlockDataSize);

/// @brief 保存数据到共享内存队列
/// @param[in] sValue 数据
/// @param[in] iPutTryTimes 尝试存储数据次数, 0无限，默认无限
/// @retval 0-成功，其他-失败
int Put(const std::string& sValue,
        int iPutTryTimes = 0);

/// @brief 保存数据到共享内存队列
/// @param[in] oReader 数据
/// @param[in] iPutTryTimes 尝试存储数据次数, 0无限，默认无限
/// @retval 0-成功，其他-失败
int Put(CQueShmReader& oReader,
        int iPutTryTimes = 0);

/// @brief 从共享内存队列拿出数据
/// @param[out] sValue 数据
/// @param[in] iPopTryTimes 尝试存储数据次数, 0无限，默认无限
/// @retval 0-成功，其他-失败
int Pop(std::string& sValue,
        int iPopTryTimes = 0);
		
/// @brief 从共享内存队列拿出数据
/// @param[out] oWriter 数据
/// @param[in] iPopTryTimes 尝试存储数据次数, 0无限，默认无限
/// @retval 0-成功，其他-失败
int Pop(CQueShmWriter& oWriter,
        int iPopTryTimes = 0);

/// @brief 共享内存队列已有数据个数
/// @param[out] uiSize 数据
/// @retval 0-成功，其他-失败
int Size(unsigned& uiSize);

/// @brief 共享内存队列是否已满
/// @param[out] bFlag 队列是否已满
/// @retval 0-成功，其他-失败
int Full(bool& bFlag);

/// @brief 共享内存队列是否已空
/// @param[out] bFlag 队列是否已空
/// @retval 0-成功，其他-失败
int Empty(bool& bFlag);

/// @brief 共享内存队列index信息
/// @param[out] uiUsedNum 共享内存已用index数量
/// @param[out] uiAllNum 共享内存所有index数量
/// @retval 0-成功，其他-失败
int GetIndexInfo(unsigned& uiUsedNum, unsigned& uiAllNum);

/// @brief 共享内存队列value block信息
/// @param[out] uiBlockDataSize block大小
/// @param[out] uiAllBlockNum block全部个数
/// @param[out] uiUsedBlockNum block使用个数
/// @retval 0-成功，其他-失败
int GetValueBlockArrayInfo(unsigned& uiBlockDataSize, unsigned& uiAllBlockNum, unsigned& uiUsedBlockNum);
```

## 定制
```cpp
class CQueShmReader
{
public:
    CQueShmReader()
    {
    }

    virtual ~CQueShmReader()
    {
    }

    /// @brief 获取要入队数据的大小，一次入队在Read前会调用一次
    /// @param[out] len 入队数据的大小
    /// @retval 0-成功，其他-失败
    virtual int Size(size_t& len) = 0;

    /// @brief 分批顺序读取入队的数据，总和为Size返回的大小
    /// @param[out] buf 入队数据分块的地址，把入队数据复制到buf上
    /// @param[in] len 入队数据分块的大小
    /// @retval 0-成功，其他-失败
    virtual int Read(void* buf, size_t len) = 0;
};

class CQueShmWriter
{
public:
    CQueShmWriter()
    {
    }

    virtual ~CQueShmWriter()
    {
    }

    /// @brief 预分配要出队数据的大小，一次出队在Write前会至少调用一次
    /// @param[in] len 出队数据的大小
    /// @retval 0-成功，其他-失败
    virtual int Reserve(size_t len) = 0;

    /// @brief 分批顺序写入出队的数据，总和为Reserve设置的大小
    /// @param[in] buf 出队数据分块的地址，从buf上把出队数据读取
    /// @param[in] len 出队数据分块的大小
    /// @retval 0-成功，其他-失败
    virtual int Write(const void* buf, size_t len) = 0;
};
```
可以根据自身数据形式，实现读写接口，定制出队入队包装，减少数据拷贝。
