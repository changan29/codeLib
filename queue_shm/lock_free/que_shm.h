
#pragma once


#include <stdint.h>

#include <cstring>
#include <string>


#define EC_QUE_SHM_LOCK_INIT_FAILED      -49001
#define EC_QUE_SHM_LOCK_FAILED           -49002
#define EC_QUE_SHM_UNLOCK_FAILED         -49003
#define EC_QUE_SHM_SHM_OPEN_FAILED       -49004
#define EC_QUE_SHM_MMAP_FAILED           -49005
#define EC_QUE_SHM_FTRUNCATE_FAILED      -49006
#define EC_QUE_SHM_FSTAT_FAILED          -49007
#define EC_QUE_SHM_MEM_NOT_ENOUGH        -49008
#define EC_QUE_SHM_DATA_ABNORMAL         -49009
#define EC_QUE_SHM_BLOCK_ABNORMAL        -49010
#define EC_QUE_SHM_MALLOC_ZERO           -49011
#define EC_QUE_SHM_OVER_TRY_TIMES        -49012
#define EC_QUE_SHM_FULL                  -49013
#define EC_QUE_SHM_EMPTY                 -49014


#define QUE_SHM_BLOCK_BEGIN 1
#define QUE_SHM_BLOCK_END 0
#define QUE_SHM_GET_BLOCK_PTR(p, s, i) ((SQueShmBlockInfo*)((uint8_t*)p + ((sizeof(SQueShmBlockInfo) + s) * i)))

#define QUE_SHM_QUE_SIZE 65536
#define QUE_SHM_VALUE_BLOCK_NUM 65536
#define QUE_SHM_VALUE_BLOCK_DATA_SIZE 4088

#pragma pack (1)

struct SQueShmHeadBlockInfo
{
    uint32_t u32Head;
    uint32_t u32Seq;    //防止cas的ABA问题
};

struct SQueShmFreeBlocksInfo
{
    SQueShmHeadBlockInfo stHeadInfo;
    uint32_t u32FreeBlockNum;   //只保证最终一致性
    uint8_t ext[4];
};

struct SQueShmInfo
{
    uint8_t head[256];
    SQueShmFreeBlocksInfo stFreeValueBlocksInfo;
    SQueShmHeadBlockInfo stReadIndex;
    SQueShmHeadBlockInfo stWriteIndex;
    uint32_t u32ValueBlockNum;
    uint32_t u32ValueBlockDataSize;
    uint32_t u32QueSize;
    uint8_t ext[724];
};

struct SQueShmBlockInfo
{
    uint32_t u32Next;
    uint32_t u32ListSize;
    uint8_t data[];
};

struct SQueShmIndexInfo
{
    SQueShmHeadBlockInfo stValueHeadInfo;
    uint32_t u32LastPutTs;
    uint32_t u32LastPopTs;
    uint8_t ext[8];
};

#pragma pack ()

struct SQueShmInfoCache
{
    SQueShmInfo* pstShmInfo;
    void* pValueBlockArray;
    SQueShmIndexInfo* pIndexArray;
    int iLockFd;
    int iShmFd;

    SQueShmInfoCache() : pstShmInfo(NULL),
    pValueBlockArray(NULL),
    pIndexArray(NULL),
    iLockFd(-1),
    iShmFd(-1)
    {
    }
};

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

class CQueShmStringReader : public CQueShmReader
{
public:
    CQueShmStringReader(const std::string& data) :
        _data(data),
        _cur(0)
    {
    }

    virtual ~CQueShmStringReader()
    {
    }

    virtual int Size(size_t& len)
    {
        len = _data.size();
        return 0;
    }

    virtual int Read(void* buf, size_t len)
    {
        memcpy(buf, _data.data() + _cur, len);
        _cur += len;
        return 0;
    }

protected:
    const std::string& _data;
    size_t _cur;
};

class CQueShmStringWriter : public CQueShmWriter
{
public:
    CQueShmStringWriter(std::string& data) :
        _data(data)
    {
    }

    virtual ~CQueShmStringWriter()
    {
    }

    virtual int Reserve(size_t len)
    {
        _data.clear();
        _data.reserve(len);
        return 0;
    }

    virtual int Write(const void* buf, size_t len)
    {
        _data.append((const char*)buf, len);
        return 0;
    }

protected:
    std::string& _data;
};

class CQueShm
{
public:
    /// @brief 构造函数
    /// @param[in] sShmName 共享内存名字
    /// @param[in] iQueSize 队列长度，可以容纳value的个数，只有在共享内存末创建时有效
    /// @param[in] iValueBlockNum value block个数，一个value可以占用多个block，只有在共享内存末创建时有效
    /// @param[in] iValueBlockDataSize value block大小，value内存分配的最小单位，只有在共享内存末创建时有效
    CQueShm(const std::string& sShmName,
            int iQueSize,
            int iValueBlockNum,
            int iValueBlockDataSize);

    virtual ~CQueShm();

    /// @brief 初始化，对外的函数都必须先调用
    /// @retval 0-成功，其他-失败
    int Init();

    /// @brief 保存数据到共享内存
    /// @param[in] sValue 数据
    /// @param[in] iPutTryTimes 尝试存储数据次数, 0无限，默认无限
    /// @retval 0-成功，其他-失败
    int Put(const std::string& sValue,
            int iPutTryTimes = 0);

    /// @brief 保存数据到共享内存
    /// @param[in] oReader 数据
    /// @param[in] iPutTryTimes 尝试存储数据次数, 0无限，默认无限
    /// @retval 0-成功，其他-失败
    int Put(CQueShmReader& oReader,
            int iPutTryTimes = 0);

    /// @brief 从共享内存拿出数据
    /// @param[out] sValue 数据
    /// @param[in] iPopTryTimes 尝试存储数据次数, 0无限，默认无限
    /// @retval 0-成功，其他-失败
    int Pop(std::string& sValue,
            int iPopTryTimes = 0);

    /// @brief 从共享内存拿出数据
    /// @param[out] oWriter 数据
    /// @param[in] iPopTryTimes 尝试存储数据次数, 0无限，默认无限
    /// @retval 0-成功，其他-失败
    int Pop(CQueShmWriter& oWriter,
            int iPopTryTimes = 0);

    /// @brief 共享内存已有数据个数
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

    /// @brief 共享内存index列信息
    /// @param[out] uiUsedNum 共享内存已用index数量
    /// @param[out] uiAllNum 共享内存所有index数量
    /// @retval 0-成功，其他-失败
    int GetIndexInfo(unsigned& uiUsedNum, unsigned& uiAllNum);

    /// @brief 共享内存value block列信息
    /// @param[out] uiBlockDataSize block大小
    /// @param[out] uiAllBlockNum block全部个数
    /// @param[out] uiUsedBlockNum block使用个数
    /// @retval 0-成功，其他-失败
    int GetValueBlockArrayInfo(unsigned& uiBlockDataSize, unsigned& uiAllBlockNum, unsigned& uiUsedBlockNum);

protected:
    /// @brief 初始化锁
    /// @retval 0-成功，其他-失败
    int InitLock();

    /// @brief 锁
    /// @retval 0-成功，其他-失败
    int Lock();

    /// @brief 解锁
    /// @retval 0-成功，其他-失败
    int Unlock();

    /// @brief 初始化共享内存
    /// @retval 0-成功，其他-失败
    int InitShm();

    /// @brief 打开共享内存并作最开始的初始化
    /// @retval 0-成功，其他-失败
    int BuildShm(int& iShmFd);

    /// @brief 存储数据
    /// @param[in] sValue 数据
    /// @param[in] iPutTryTimes 尝试存储数据次数, 0无限，默认1次
    /// @retval 0-成功，其他-失败
    int Put(int iBlockIndex,
            int iPutTryTimes);

    /// @brief 从block队列申请空间
    /// @param[in] size 申请空间大小
    /// @param[out] iBlockIndex 申请到block的index
    /// @retval 0-成功，其他-失败
    int MallocBlock(size_t size, int& iBlockIndex);

    /// @brief 找最后block的index
    /// @param[in] u32Head 开始的block的index
    /// @param[in] size 空间大小
    /// @param[out] iTailIndex 最后的block的index
    /// @retval 0-成功，其他-失败
    int GetTailBlockIndex(uint32_t u32Head,
                          size_t size,
                          unsigned& uiTailIndex);

    /// @brief 释放空间回block队列
    /// @param[in] u32Head 开始的block的index
    void FreeBlock(uint32_t u32Head);

    /// @brief 把数据分片保存到block
    /// @param[in] pValue 数据指针
    /// @param[in] size 空间大小
    /// @param[in] u32Head 开始的block的index
    /// @retval 0-成功，其他-失败
    int Fragment(CQueShmReader& oReader,
                 size_t size,
                 uint32_t u32Head);

    /// @brief 把block的分片数据重组
    /// @param[in] u32Head 开始的block的index
    /// @param[in] pValue 数据指针
    /// @param[in] size 空间大小
    /// @retval 0-成功，其他-失败
    int Reassemble(uint32_t u32Head,
                   size_t size,
                   CQueShmWriter& oWriter);

    bool Full(uint32_t u32ReadIndex, uint32_t u32WriteIndex);

    bool Empty(uint32_t u32ReadIndex, uint32_t u32WriteIndex);

    unsigned Size(uint32_t u32ReadIndex, uint32_t u32WriteIndex);

    uint32_t NextIndex(uint32_t u32CurIndex);

    void TryFixReadIndex();

    void TryFixWriteIndex();

    static void ThreadLock();

    static void ThreadUnLock();

    static SQueShmInfoCache& GetQueShmInfoCache(const std::string& sShmName);

protected:
    std::string _sShmName;

    int _iQueSize;
    int _iValueBlockNum;
    int _iValueBlockDataSize;

    bool _bIsInit;

    SQueShmInfoCache& _stShmInfo;
};

