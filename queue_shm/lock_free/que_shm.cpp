#include "que_shm.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/syscall.h>

#include <map>

#include "CAS_64bit.h"
#define QUE_SHM_CAS CAS_64bit
#define QUE_SHM_INT_TYPE uint64_t

#define QUE_SHM_FAA __sync_fetch_and_add
#define QUE_SHM_FAS __sync_fetch_and_sub

#define QUE_SHM_LOCK_FILE    "/dev/shm/."
#define QUE_SHM_DEV_SHM_PATH     "/dev/shm"

//#define QUE_SHM_UPDATE_LAST_TS

static int g_iQueShmThreadLock = 0;

CQueShm::CQueShm(const std::string& sShmName,
                 int iQueSize,
                 int iValueBlockNum,
                 int iValueBlockDataSize) :
    _sShmName("queshm_" + sShmName),
    _iQueSize(iQueSize),
    _iValueBlockNum(iValueBlockNum + 1),  //第0块没用
    _iValueBlockDataSize(iValueBlockDataSize),
    _bIsInit(false),
    _stShmInfo(GetQueShmInfoCache(sShmName))
{
    if(1 > _iQueSize) _iQueSize = QUE_SHM_QUE_SIZE;
    if(1 > _iValueBlockNum) _iValueBlockNum = QUE_SHM_VALUE_BLOCK_NUM + 1;
    if(0 >= _iValueBlockDataSize) _iValueBlockDataSize = QUE_SHM_VALUE_BLOCK_DATA_SIZE;

    int iValueBlockDataSizeRemainder = _iValueBlockDataSize % sizeof(QUE_SHM_INT_TYPE);
    if(iValueBlockDataSizeRemainder > 0) _iValueBlockDataSize += sizeof(QUE_SHM_INT_TYPE) - iValueBlockDataSizeRemainder;
}

CQueShm::~CQueShm()
{
}

int CQueShm::Init()
{
    if(_bIsInit)
    {
        return 0;
    }

    int iRet = 0;

    ThreadLock();

    if(_bIsInit)
    {
        ThreadUnLock();
        return 0;
    }

    iRet = InitLock();
    if(0 != iRet)
    {
        ThreadUnLock();
        return iRet;
    }

    iRet = InitShm();
    if(0 != iRet)
    {
        ThreadUnLock();
        return iRet;
    }

    _bIsInit = true;

    ThreadUnLock();

    return 0;
}

int CQueShm::InitLock()
{
    if(_stShmInfo.iLockFd >= 0)
    {
        return 0;
    }

    char szLockFile[256];
    snprintf(szLockFile, sizeof(szLockFile), "%s_%s_lock", QUE_SHM_LOCK_FILE, _sShmName.c_str());
    int fd = open(szLockFile, O_RDONLY | O_CREAT, 0644);
    if(0 > fd)
    {
        return EC_QUE_SHM_LOCK_INIT_FAILED;
    }
    _stShmInfo.iLockFd = fd;

    return 0;
}

int CQueShm::Lock()
{
    return flock(_stShmInfo.iLockFd, LOCK_EX) == 0 ? 0 : EC_QUE_SHM_LOCK_FAILED;
}

int CQueShm::Unlock()
{
    return flock(_stShmInfo.iLockFd, LOCK_UN) == 0 ? 0 : EC_QUE_SHM_UNLOCK_FAILED;
}

void CQueShm::ThreadLock()
{
    while(__sync_lock_test_and_set(&g_iQueShmThreadLock, 1)) {}
}

void CQueShm::ThreadUnLock()
{
    __sync_lock_release(&g_iQueShmThreadLock);
}

int CQueShm::InitShm()
{
    if(0 <= _stShmInfo.iShmFd
       && NULL != _stShmInfo.pstShmInfo
       && NULL != _stShmInfo.pValueBlockArray
       && NULL != _stShmInfo.pIndexArray)
    {
        return 0;
    }

    int iRet = 0;

    int fd = shm_open(_sShmName.c_str(), O_RDWR, 0777);
    if(0 > fd)
    {
        int iLockRet = 0;
        iLockRet = Lock();
        if(0 != iLockRet)
        {
            return iLockRet;
        }

        iRet = BuildShm(fd);

        iLockRet = Unlock();
        if(0 != iLockRet)
        {
            if(0 == iRet)
            {
                close(fd);
            }
            return iLockRet;
        }

        if(0 != iRet)
        {
            return iRet;
        }
    }

    struct stat buf;
    iRet = fstat(fd, &buf);
    if(0 > iRet)
    {
        close(fd);
        return EC_QUE_SHM_FSTAT_FAILED;
    }

    size_t size = buf.st_size;

    void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, SEEK_SET);
    if(p == NULL)
    {
        close(fd);
        return EC_QUE_SHM_MMAP_FAILED;
    }

    //共享内存信息记录
    _stShmInfo.iShmFd = fd;
    _stShmInfo.pstShmInfo = (SQueShmInfo*)p;
    _stShmInfo.pValueBlockArray = (uint8_t*)_stShmInfo.pstShmInfo + sizeof(SQueShmInfo);
    _stShmInfo.pIndexArray = (SQueShmIndexInfo*)((uint8_t*)_stShmInfo.pValueBlockArray + _stShmInfo.pstShmInfo->u32ValueBlockNum * (sizeof(SQueShmBlockInfo) + _stShmInfo.pstShmInfo->u32ValueBlockDataSize));

    return 0;
}

int CQueShm::BuildShm(int& iShmFd)
{
    int iRet = 0;

    size_t size = sizeof(SQueShmInfo) + _iQueSize * sizeof(SQueShmIndexInfo)
        + _iValueBlockNum * (sizeof(SQueShmBlockInfo) + _iValueBlockDataSize);

    int fd = shm_open(_sShmName.c_str(), O_RDWR, 0777);
    if(0 > fd)
    {
        char szShmNameTemp[256];
        snprintf(szShmNameTemp, sizeof(szShmNameTemp), "%s.%ld", _sShmName.c_str(), syscall(SYS_gettid));

        char szPathOld[256] = {0};
        char szPathNew[256] = {0};
        snprintf(szPathOld, sizeof(szPathOld), "%s/%s", QUE_SHM_DEV_SHM_PATH, szShmNameTemp);
        snprintf(szPathNew, sizeof(szPathNew), "%s/%s", QUE_SHM_DEV_SHM_PATH, _sShmName.c_str());

        unlink(szPathOld);

        fd = shm_open(szShmNameTemp, O_CREAT | O_RDWR, 0777);
        if(0 > fd)
        {
            return EC_QUE_SHM_SHM_OPEN_FAILED;
        }

        iRet = ftruncate(fd, size);
        if(0 > iRet)
        {
            close(fd);
            unlink(szPathOld);
            return EC_QUE_SHM_FTRUNCATE_FAILED;
        }

        iRet = lseek(fd, 0, SEEK_SET);
        if(0 > iRet)
        {
            close(fd);
            unlink(szPathOld);
            return EC_QUE_SHM_FTRUNCATE_FAILED;
        }

        int bs = 4096;
        char* b = new char[bs];
        memset(b, 0, bs);
        size_t n = 0;
        while(n < size)
        {
            int l = n + bs > size ? size - n : bs;
            iRet = write(fd, b, l);
            if(l > iRet)
            {
                delete[] b;
                close(fd);
                unlink(szPathOld);
                return EC_QUE_SHM_FTRUNCATE_FAILED;
            }
            n += l;
        }
        delete[] b;

        void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, SEEK_SET);
        if(NULL == addr || (void*)-1 == addr)
        {
            close(fd);
            unlink(szPathOld);
            return EC_QUE_SHM_MMAP_FAILED;
        }

        memset(addr, 0, size);

        SQueShmInfo* pShmInfo = (SQueShmInfo*)addr;
        pShmInfo->stFreeValueBlocksInfo.stHeadInfo.u32Head = _iValueBlockNum <= QUE_SHM_BLOCK_BEGIN ? QUE_SHM_BLOCK_END : QUE_SHM_BLOCK_BEGIN;
        pShmInfo->stFreeValueBlocksInfo.u32FreeBlockNum = _iValueBlockNum <= QUE_SHM_BLOCK_BEGIN ? 0 : _iValueBlockNum - QUE_SHM_BLOCK_BEGIN;
        pShmInfo->u32ValueBlockNum = _iValueBlockNum;
        pShmInfo->u32ValueBlockDataSize = _iValueBlockDataSize;
        pShmInfo->u32QueSize = _iQueSize;

        void* pValueBlock = (uint8_t*)pShmInfo + sizeof(SQueShmInfo);
        for(int i = QUE_SHM_BLOCK_BEGIN; i < _iValueBlockNum - 1; ++i)
        {
            QUE_SHM_GET_BLOCK_PTR(pValueBlock, _iValueBlockDataSize, i)->u32Next = i+1;
        }

        munmap(addr, size);

        close(fd);

        link(szPathOld, szPathNew);

        unlink(szPathOld);

        fd = shm_open(_sShmName.c_str(), O_RDWR, 0777);
        if(0 > fd)
        {
            return EC_QUE_SHM_SHM_OPEN_FAILED;
        }
    }

    iShmFd = fd;

    return 0;
}

int CQueShm::Size(unsigned& uiSize)
{
    int iRet = 0;

    iRet = Init();
    if(0 != iRet)
    {
        return iRet;
    }

    uiSize = Size(_stShmInfo.pstShmInfo->stReadIndex.u32Head, _stShmInfo.pstShmInfo->stWriteIndex.u32Head);

    return 0;
}

unsigned CQueShm::Size(uint32_t u32ReadIndex, uint32_t u32WriteIndex)
{
    if(u32WriteIndex < u32ReadIndex)
    {
        return u32WriteIndex + _stShmInfo.pstShmInfo->u32QueSize - u32ReadIndex;
    }
    else if(u32WriteIndex > u32ReadIndex)
    {
        return u32WriteIndex - u32ReadIndex;
    }
    else
    {
        return 0;
    }
}

int CQueShm::Full(bool& bFlag)
{
    int iRet = 0;

    iRet = Init();
    if(0 != iRet)
    {
        return iRet;
    }

    bFlag = Full(_stShmInfo.pstShmInfo->stReadIndex.u32Head, _stShmInfo.pstShmInfo->stWriteIndex.u32Head);

    return 0;
}

bool CQueShm::Full(uint32_t u32ReadIndex, uint32_t u32WriteIndex)
{
    return Size(u32ReadIndex, u32WriteIndex) + 1 >= _stShmInfo.pstShmInfo->u32QueSize;
#if 0
    if(u32WriteIndex < u32ReadIndex)
    {
        if(u32WriteIndex + 1 >= u32ReadIndex)
        {
            return true;
        };
    }
    else if(u32WriteIndex > u32ReadIndex)
    {
        if(u32WriteIndex + 1 >= _stShmInfo.pstShmInfo->u32QueSize + u32ReadIndex)
        {
            return true;
        }
    }
    else
    {
        return false;
    }

    return false;
#endif
}

int CQueShm::Empty(bool& bFlag)
{
    int iRet = 0;

    iRet = Init();
    if(0 != iRet)
    {
        return iRet;
    }

    bFlag = Empty(_stShmInfo.pstShmInfo->stReadIndex.u32Head, _stShmInfo.pstShmInfo->stWriteIndex.u32Head);

    return 0;
}

bool CQueShm::Empty(uint32_t u32ReadIndex, uint32_t u32WriteIndex)
{
    return Size(u32ReadIndex, u32WriteIndex) == 0;
}

uint32_t CQueShm::NextIndex(uint32_t u32CurIndex)
{
    return (u32CurIndex + 1) % _stShmInfo.pstShmInfo->u32QueSize;
}

void CQueShm::TryFixReadIndex()
{
    /*
    if(Empty(_stShmInfo.pstShmInfo->stReadIndex.u32Head, _stShmInfo.pstShmInfo->stWriteIndex.u32Head))
    {
        return;
    }
    */

    //快速退出
    unsigned uiIndexCheck = NextIndex(_stShmInfo.pstShmInfo->stReadIndex.u32Head);
    if(QUE_SHM_BLOCK_END != _stShmInfo.pIndexArray[uiIndexCheck].stValueHeadInfo.u32Head)
    {
        return;
    }

    SQueShmHeadBlockInfo stReadIndexOld;

    QUE_SHM_CAS(&stReadIndexOld, stReadIndexOld, _stShmInfo.pstShmInfo->stReadIndex);

    if(Empty(stReadIndexOld.u32Head, _stShmInfo.pstShmInfo->stWriteIndex.u32Head))
    {
        return;
    }

    unsigned uiIndex = NextIndex(stReadIndexOld.u32Head);

    if(QUE_SHM_BLOCK_END != _stShmInfo.pIndexArray[uiIndex].stValueHeadInfo.u32Head)
    {
        return;
    }

    SQueShmHeadBlockInfo stReadIndexNew;
    stReadIndexNew.u32Head = uiIndex;
    stReadIndexNew.u32Seq = stReadIndexOld.u32Seq + 1;

    if(QUE_SHM_CAS(&_stShmInfo.pstShmInfo->stReadIndex, stReadIndexOld, stReadIndexNew))
    {
#ifdef QUE_SHM_UPDATE_LAST_TS
        _stShmInfo.pIndexArray[uiIndex].u32LastPopTs = time(NULL);
#endif
    }

    return;
}

void CQueShm::TryFixWriteIndex()
{
    /*
    if(Full(_stShmInfo.pstShmInfo->stReadIndex.u32Head, _stShmInfo.pstShmInfo->stWriteIndex.u32Head))
    {
        return;
    }
    */

    //快速退出
    unsigned uiIndexCheck = NextIndex(_stShmInfo.pstShmInfo->stWriteIndex.u32Head);
    if(QUE_SHM_BLOCK_END == _stShmInfo.pIndexArray[uiIndexCheck].stValueHeadInfo.u32Head)
    {
        return;
    }

    SQueShmHeadBlockInfo stWriteIndexOld;

    QUE_SHM_CAS(&stWriteIndexOld, stWriteIndexOld, _stShmInfo.pstShmInfo->stWriteIndex);

    if(Full(_stShmInfo.pstShmInfo->stReadIndex.u32Head, stWriteIndexOld.u32Head))
    {
        return;
    }

    unsigned uiIndex = NextIndex(stWriteIndexOld.u32Head);

    if(QUE_SHM_BLOCK_END == _stShmInfo.pIndexArray[uiIndex].stValueHeadInfo.u32Head)
    {
        return;
    }

    SQueShmHeadBlockInfo stWriteIndexNew;
    stWriteIndexNew.u32Head = uiIndex;
    stWriteIndexNew.u32Seq = stWriteIndexOld.u32Seq + 1;

    if(QUE_SHM_CAS(&_stShmInfo.pstShmInfo->stWriteIndex, stWriteIndexOld, stWriteIndexNew))
    {
#ifdef QUE_SHM_UPDATE_LAST_TS
        _stShmInfo.pIndexArray[uiIndex].u32LastPutTs = time(NULL);
#endif
    }

    return;
}

int CQueShm::Put(const std::string& sValue,
                 int iPutTryTimes)
{
    CQueShmStringReader oReader(sValue);
    return Put(oReader, iPutTryTimes);
}

int CQueShm::Put(CQueShmReader& oReader,
                 int iPutTryTimes)
{
    int iRet = 0;

    iRet = Init();
    if(0 != iRet)
    {
        return iRet;
    }

    TryFixReadIndex();

    if(Full(_stShmInfo.pstShmInfo->stReadIndex.u32Head, _stShmInfo.pstShmInfo->stWriteIndex.u32Head))
    {
        SQueShmHeadBlockInfo stWriteIndexOld;

        for(;;)
        {
            QUE_SHM_CAS(&stWriteIndexOld, stWriteIndexOld, _stShmInfo.pstShmInfo->stWriteIndex);

            if(Full(_stShmInfo.pstShmInfo->stReadIndex.u32Head, stWriteIndexOld.u32Head))
            {
                if(stWriteIndexOld.u32Head != _stShmInfo.pstShmInfo->stWriteIndex.u32Head
                   || stWriteIndexOld.u32Seq != _stShmInfo.pstShmInfo->stWriteIndex.u32Seq)
                {
                    continue;
                }

                return EC_QUE_SHM_FULL;
            }

            break;
        }
    }

    size_t size = 0;

    iRet = oReader.Size(size);
    if(0 != iRet)
    {
        return iRet;
    }

    int iBlockIndex = 0;
    iRet = MallocBlock(size, iBlockIndex);
    if(0 != iRet)
    {
        return iRet;
    }

    iRet = Fragment(oReader,
                    size,
                    iBlockIndex);
    if(0 != iRet)
    {
        if(EC_QUE_SHM_BLOCK_ABNORMAL != iRet)
        {
            FreeBlock(iBlockIndex);
        }
        return iRet;
    }

    iRet = Put(iBlockIndex,
               iPutTryTimes);

    if(0 != iRet)
    {
        FreeBlock(iBlockIndex);

        return iRet;
    }

    return 0;
}

int CQueShm::Put(int iBlockIndex,
                 int iPutTryTimes)
{
    int iTryTimes = 0;

    SQueShmHeadBlockInfo stBlocksInfoOld;
    SQueShmHeadBlockInfo stBlocksInfoNew;

    SQueShmHeadBlockInfo stWriteIndexOld;

    for(;;)
    {
        if(0 < iPutTryTimes
           && ++iTryTimes > iPutTryTimes)
        {
            return EC_QUE_SHM_OVER_TRY_TIMES;
        }

        QUE_SHM_CAS(&stWriteIndexOld, stWriteIndexOld, _stShmInfo.pstShmInfo->stWriteIndex);

        if(Full(_stShmInfo.pstShmInfo->stReadIndex.u32Head, stWriteIndexOld.u32Head))
        {
            if(stWriteIndexOld.u32Head != _stShmInfo.pstShmInfo->stWriteIndex.u32Head
               || stWriteIndexOld.u32Seq != _stShmInfo.pstShmInfo->stWriteIndex.u32Seq)
            {
                continue;
            }

            return EC_QUE_SHM_FULL;
        }

        unsigned uiIndex = NextIndex(stWriteIndexOld.u32Head);

        SQueShmHeadBlockInfo* pstHeadInfo = &_stShmInfo.pIndexArray[uiIndex].stValueHeadInfo;

        QUE_SHM_CAS(&stBlocksInfoOld, stBlocksInfoOld, *pstHeadInfo);

        if(QUE_SHM_BLOCK_END != stBlocksInfoOld.u32Head)
        {
            TryFixWriteIndex();
            continue;
        }

        if(stWriteIndexOld.u32Head != _stShmInfo.pstShmInfo->stWriteIndex.u32Head
           || stWriteIndexOld.u32Seq != _stShmInfo.pstShmInfo->stWriteIndex.u32Seq)
        {
            continue;
        }

        stBlocksInfoNew.u32Head = iBlockIndex;
        stBlocksInfoNew.u32Seq = stBlocksInfoOld.u32Seq + 1;

        if(QUE_SHM_CAS(pstHeadInfo, stBlocksInfoOld, stBlocksInfoNew))
        {
            SQueShmHeadBlockInfo stWriteIndexNew;
            stWriteIndexNew.u32Head = uiIndex;
            stWriteIndexNew.u32Seq = stWriteIndexOld.u32Seq + 1;

            if(QUE_SHM_CAS(&_stShmInfo.pstShmInfo->stWriteIndex, stWriteIndexOld, stWriteIndexNew))
            {
#ifdef QUE_SHM_UPDATE_LAST_TS
                _stShmInfo.pIndexArray[uiIndex].u32LastPutTs = time(NULL);
#endif
            }

            return 0;
        }
        else
        {
            continue;
        }
    }

    return 0;
}

int CQueShm::Pop(std::string& sValue,
                 int iPopTryTimes)
{
    CQueShmStringWriter oWriter(sValue);
    return Pop(oWriter, iPopTryTimes);
}

int CQueShm::Pop(CQueShmWriter& oWriter,
                 int iPopTryTimes)
{
    int iRet = 0;

    int iTryTimes = 0;

    iRet = Init();
    if(0 != iRet)
    {
        return iRet;
    }

    TryFixWriteIndex();

    SQueShmHeadBlockInfo stBlocksInfoOld;
    SQueShmHeadBlockInfo stBlocksInfoNew;

    SQueShmHeadBlockInfo stReadIndexOld;

    for(;;)
    {
        if(0 < iPopTryTimes
           && ++iTryTimes > iPopTryTimes)
        {
            return EC_QUE_SHM_OVER_TRY_TIMES;
        }

        QUE_SHM_CAS(&stReadIndexOld, stReadIndexOld, _stShmInfo.pstShmInfo->stReadIndex);

        if(Empty(_stShmInfo.pstShmInfo->stWriteIndex.u32Head, stReadIndexOld.u32Head))
        {
            if(stReadIndexOld.u32Head != _stShmInfo.pstShmInfo->stReadIndex.u32Head
               || stReadIndexOld.u32Seq != _stShmInfo.pstShmInfo->stReadIndex.u32Seq)
            {
                continue;
            }

            return EC_QUE_SHM_EMPTY;
        }

        unsigned uiIndex = NextIndex(stReadIndexOld.u32Head);

        SQueShmHeadBlockInfo* pstHeadInfo = &_stShmInfo.pIndexArray[uiIndex].stValueHeadInfo;

        QUE_SHM_CAS(&stBlocksInfoOld, stBlocksInfoOld, *pstHeadInfo);

        if(QUE_SHM_BLOCK_END == stBlocksInfoOld.u32Head)
        {
            TryFixReadIndex();
            continue;
        }

        if(stReadIndexOld.u32Head != _stShmInfo.pstShmInfo->stReadIndex.u32Head
           || stReadIndexOld.u32Seq != _stShmInfo.pstShmInfo->stReadIndex.u32Seq)
        {
            continue;
        }

        stBlocksInfoNew.u32Head = 0;
        stBlocksInfoNew.u32Seq = stBlocksInfoOld.u32Seq + 1;

        uint32_t u32ValueSize = QUE_SHM_GET_BLOCK_PTR(_stShmInfo.pValueBlockArray, _stShmInfo.pstShmInfo->u32ValueBlockDataSize, stBlocksInfoOld.u32Head)->u32ListSize;
        if(0 == u32ValueSize)
        {
            QUE_SHM_CAS(pstHeadInfo, stBlocksInfoOld, stBlocksInfoNew);
            continue;
        }

        iRet = oWriter.Reserve(u32ValueSize);
        if(0 != iRet)
        {
            return iRet;
        }

        if(QUE_SHM_CAS(pstHeadInfo, stBlocksInfoOld, stBlocksInfoNew))
        {
            SQueShmHeadBlockInfo stReadIndexNew;
            stReadIndexNew.u32Head = uiIndex;
            stReadIndexNew.u32Seq = stReadIndexOld.u32Seq + 1;

            if(QUE_SHM_CAS(&_stShmInfo.pstShmInfo->stReadIndex, stReadIndexOld, stReadIndexNew))
            {
#ifdef QUE_SHM_UPDATE_LAST_TS
                _stShmInfo.pIndexArray[uiIndex].u32LastPopTs = time(NULL);
#endif
            }

            iRet = Reassemble(stBlocksInfoOld.u32Head, u32ValueSize, oWriter);
            if(0 != iRet)
            {
                if(EC_QUE_SHM_DATA_ABNORMAL != iRet)
                {
                    FreeBlock(stBlocksInfoOld.u32Head);
                }
                return iRet;
            }
            FreeBlock(stBlocksInfoOld.u32Head);

            return 0;
        }
        else
        {
            continue;
        }
    }

    return 0;
}

int CQueShm::MallocBlock(size_t size, int& iBlockIndex)
{
    if(0 == size)
    {
        return EC_QUE_SHM_MALLOC_ZERO;
    }

    int iRet = 0;
    SQueShmHeadBlockInfo stBlocksInfoOld;
    SQueShmHeadBlockInfo stBlocksInfoNew;
    for(;;)
    {
        QUE_SHM_CAS(&stBlocksInfoOld, stBlocksInfoOld, _stShmInfo.pstShmInfo->stFreeValueBlocksInfo.stHeadInfo);

        unsigned uiTailIndex = 0;
        iRet = GetTailBlockIndex(stBlocksInfoOld.u32Head, size, uiTailIndex);
        if(0 != iRet)
        {
            if(*(QUE_SHM_INT_TYPE*)&stBlocksInfoOld == *(QUE_SHM_INT_TYPE*)&_stShmInfo.pstShmInfo->stFreeValueBlocksInfo.stHeadInfo)
            {
                return iRet;
            }
            else
            {
                continue;
            }
        }

        SQueShmBlockInfo* pTailBlock = QUE_SHM_GET_BLOCK_PTR(_stShmInfo.pValueBlockArray, _stShmInfo.pstShmInfo->u32ValueBlockDataSize, uiTailIndex);

        stBlocksInfoNew.u32Head = pTailBlock->u32Next;
        stBlocksInfoNew.u32Seq = stBlocksInfoOld.u32Seq + 1;

        if(QUE_SHM_CAS(&_stShmInfo.pstShmInfo->stFreeValueBlocksInfo.stHeadInfo, stBlocksInfoOld, stBlocksInfoNew))
        {
            SQueShmBlockInfo* pHeadBlock = QUE_SHM_GET_BLOCK_PTR(_stShmInfo.pValueBlockArray, _stShmInfo.pstShmInfo->u32ValueBlockDataSize, stBlocksInfoOld.u32Head);
            pHeadBlock->u32ListSize = size;

            pTailBlock->u32Next = QUE_SHM_BLOCK_END;

            uint32_t u32UsedNum = (size + _stShmInfo.pstShmInfo->u32ValueBlockDataSize - 1) / _stShmInfo.pstShmInfo->u32ValueBlockDataSize;
            QUE_SHM_FAS(&_stShmInfo.pstShmInfo->stFreeValueBlocksInfo.u32FreeBlockNum, u32UsedNum);

            iBlockIndex = stBlocksInfoOld.u32Head;

            return 0;
        }
    }
}

int CQueShm::GetTailBlockIndex(uint32_t u32Head,
                               size_t size,
                               unsigned& uiTailIndex)
{
    if(u32Head == QUE_SHM_BLOCK_END)
    {
        return EC_QUE_SHM_MEM_NOT_ENOUGH;
    }

    size_t siBlockSize = 0;
    unsigned uiIndex = u32Head;
    do
    {
        siBlockSize += _stShmInfo.pstShmInfo->u32ValueBlockDataSize;
        if(siBlockSize >= size)
        {
            break;
        }

        uiIndex = QUE_SHM_GET_BLOCK_PTR(_stShmInfo.pValueBlockArray, _stShmInfo.pstShmInfo->u32ValueBlockDataSize, uiIndex)->u32Next;
    }while(uiIndex != QUE_SHM_BLOCK_END);

    if(siBlockSize >= size)
    {
        uiTailIndex = uiIndex;
        return 0;
    }

    return EC_QUE_SHM_MEM_NOT_ENOUGH;
}

void CQueShm::FreeBlock(uint32_t u32Head)
{
    if(QUE_SHM_BLOCK_END == u32Head)
    {
        return;
    }

    SQueShmBlockInfo* pTailBlock = QUE_SHM_GET_BLOCK_PTR(_stShmInfo.pValueBlockArray, _stShmInfo.pstShmInfo->u32ValueBlockDataSize, u32Head);
    for(; ;)
    {
        if(QUE_SHM_BLOCK_END == pTailBlock->u32Next)
        {
            break;
        }
        pTailBlock = QUE_SHM_GET_BLOCK_PTR(_stShmInfo.pValueBlockArray, _stShmInfo.pstShmInfo->u32ValueBlockDataSize, pTailBlock->u32Next);
    }

    SQueShmBlockInfo* pHeadBlock = QUE_SHM_GET_BLOCK_PTR(_stShmInfo.pValueBlockArray, _stShmInfo.pstShmInfo->u32ValueBlockDataSize, u32Head);
    uint32_t u32FreeBlockNum = (pHeadBlock->u32ListSize + _stShmInfo.pstShmInfo->u32ValueBlockDataSize - 1) / _stShmInfo.pstShmInfo->u32ValueBlockDataSize;
    SQueShmHeadBlockInfo stBlocksInfoOld;
    SQueShmHeadBlockInfo stBlocksInfoNew;
    for(;;)
    {
        QUE_SHM_CAS(&stBlocksInfoOld, stBlocksInfoOld, _stShmInfo.pstShmInfo->stFreeValueBlocksInfo.stHeadInfo);

        pTailBlock->u32Next = stBlocksInfoOld.u32Head;
        pHeadBlock->u32ListSize = 0;
        stBlocksInfoNew.u32Head = u32Head;
        stBlocksInfoNew.u32Seq = stBlocksInfoOld.u32Seq + 1;

        if(QUE_SHM_CAS(&_stShmInfo.pstShmInfo->stFreeValueBlocksInfo.stHeadInfo, stBlocksInfoOld, stBlocksInfoNew))
        {
            QUE_SHM_FAA(&_stShmInfo.pstShmInfo->stFreeValueBlocksInfo.u32FreeBlockNum, u32FreeBlockNum);
            return;
        }
    }
}

int CQueShm::Fragment(CQueShmReader& oReader,
                      size_t size,
                      uint32_t u32Head)
{
    int iRet = 0;
    size_t offset = 0;
    unsigned uiIndex = u32Head;
    do
    {
        SQueShmBlockInfo* pBlock = QUE_SHM_GET_BLOCK_PTR(_stShmInfo.pValueBlockArray, _stShmInfo.pstShmInfo->u32ValueBlockDataSize, uiIndex);
        size_t siCopySize = size < offset + _stShmInfo.pstShmInfo->u32ValueBlockDataSize ? size - offset : _stShmInfo.pstShmInfo->u32ValueBlockDataSize;
        iRet = oReader.Read(pBlock->data, siCopySize);
        if(0 != iRet)
        {
            return iRet;
        }
        offset += siCopySize;
        uiIndex = pBlock->u32Next;
    }while(uiIndex != QUE_SHM_BLOCK_END && offset < size);

    if(QUE_SHM_BLOCK_END != uiIndex || offset != size)
    {
        return EC_QUE_SHM_BLOCK_ABNORMAL;
    }

    return 0;
}

int CQueShm::Reassemble(uint32_t u32Head,
                        size_t size,
                        CQueShmWriter& oWriter)
{
    int iRet = 0;
    size_t offset = 0;
    unsigned uiIndex = u32Head;
    do
    {
        SQueShmBlockInfo* pBlock = QUE_SHM_GET_BLOCK_PTR(_stShmInfo.pValueBlockArray, _stShmInfo.pstShmInfo->u32ValueBlockDataSize, uiIndex);
        size_t siCopySize = size < offset + _stShmInfo.pstShmInfo->u32ValueBlockDataSize ? size - offset : _stShmInfo.pstShmInfo->u32ValueBlockDataSize;
        iRet = oWriter.Write(pBlock->data, siCopySize);
        if(0 != iRet)
        {
            return iRet;
        }
        offset += siCopySize;
        uiIndex = pBlock->u32Next;
    }while(uiIndex != QUE_SHM_BLOCK_END);

    if(QUE_SHM_BLOCK_END != uiIndex || offset != size)
    {
        return EC_QUE_SHM_DATA_ABNORMAL;
    }

    return 0;
}

int CQueShm::GetIndexInfo(unsigned& uiUsedNum, unsigned& uiAllNum)
{
    int iRet = 0;

    iRet = Init();
    if(0 != iRet)
    {
        return iRet;
    }

    uiAllNum = _stShmInfo.pstShmInfo->u32QueSize;
    uiUsedNum = Size(_stShmInfo.pstShmInfo->stReadIndex.u32Head, _stShmInfo.pstShmInfo->stWriteIndex.u32Head);

    return 0;
}

int CQueShm::GetValueBlockArrayInfo(unsigned& uiBlockDataSize, unsigned& uiAllBlockNum, unsigned& uiUsedBlockNum)
{
    int iRet = 0;

    iRet = Init();
    if(0 != iRet)
    {
        return iRet;
    }

    uiBlockDataSize = _stShmInfo.pstShmInfo->u32ValueBlockDataSize;
    uiAllBlockNum = _stShmInfo.pstShmInfo->u32ValueBlockNum - QUE_SHM_BLOCK_BEGIN;
    uiUsedBlockNum = uiAllBlockNum - _stShmInfo.pstShmInfo->stFreeValueBlocksInfo.u32FreeBlockNum;

    return 0;
}

SQueShmInfoCache& CQueShm::GetQueShmInfoCache(const std::string& sShmName)
{
    ThreadLock();

    static std::map<std::string, SQueShmInfoCache> mapShmInfo;    //所有共享内存信息

    SQueShmInfoCache& stShmInfo = mapShmInfo[sShmName];

    ThreadUnLock();

    return stShmInfo;
}

