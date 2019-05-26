#include "que_shm.h"
#include <stdlib.h>
#include <sys/time.h>
#include <fstream>
#include <string>

using namespace std;

void help(char** argv)
{
    printf("%s shm_key put value\n", argv[0]);
    printf("%s shm_key pop\n", argv[0]);
    printf("%s shm_key test_put num\n", argv[0]);
    printf("%s shm_key test_pop num\n", argv[0]);
    printf("%s shm_key loop_put num\n", argv[0]);
    printf("%s shm_key loop_pop num [file_name]\n", argv[0]);
}

int main(int argc, char** argv)
{
    if(argc < 3)
    {
        help(argv);
        return -1;
    }

    CQueShm que(argv[1], 100000, 100000, 10);

    string key = argv[2];

    int ret = 0;

    string v;
    if(key == "put")
    {
        if(argc < 4)
        {
            help(argv);
            return -1;
        }
        v = argv[3];
        ret = que.Put(v);
        printf("ret=%d, v=%s\n", ret, v.c_str());
    }
    else if(key == "pop")
    {
        ret = que.Pop(v);
        printf("ret=%d, v=%s\n", ret, v.c_str());
    }
    else if(key == "test_put")
    {
        if(argc < 4)
        {
            help(argv);
            return -1;
        }
        int n = atol(argv[3]);
        int f = 0;

        struct timeval b;
        gettimeofday(&b, NULL);
        for (int i = 0; i < n; i++)
        {
            v.assign((char*)&i, sizeof(i));
            for(;;)
            {
                ret = que.Put(v);
                if(0 == ret)
                {
                    break;
                }
                ++f;
            }
        }
        struct timeval e;
        gettimeofday(&e, NULL);
        printf("f=%d, use_us=%jd\n", f, (int64_t)e.tv_sec*1000000 - (int64_t)b.tv_sec*1000000 + (e.tv_usec - b.tv_usec));
    }
    else if(key == "test_pop")
    {
        if(argc < 4)
        {
            help(argv);
            return -1;
        }

        int n = atol(argv[3]);
        int f = 0;

        struct timeval b;
        gettimeofday(&b, NULL);
        for (int i = 0; i < n; i++)
        {
            for(;;)
            {
                ret = que.Pop(v);
                if(0 == ret)
                {
                    break;
                }
                ++f;
            }
        }
        struct timeval e;
        gettimeofday(&e, NULL);
        printf("e=%d, use_us=%jd\n", f, (int64_t)e.tv_sec*1000000 - (int64_t)b.tv_sec*1000000 + (e.tv_usec - b.tv_usec));
    }
    else if(key == "loop_put")
    {
        if(argc < 4)
        {
            help(argv);
            return -1;
        }
        int n = atol(argv[3]);

        int* fa = new int[EC_QUE_SHM_LOCK_INIT_FAILED - EC_QUE_SHM_EMPTY + 1];
        int be = n / 10 * 2;
        struct timeval b;
        gettimeofday(&b, NULL);
        int f = 0;
        for (int i = 0; i < n; i++)
        {
            v.assign((char*)&i, sizeof(i));
            for(;;)
            {
                ret = que.Put(v);
                if(0 == ret)
                {
                    break;
                }
                ++f;
                if(i > be && ret <= EC_QUE_SHM_LOCK_INIT_FAILED && ret >= EC_QUE_SHM_EMPTY)
                    ++fa[EC_QUE_SHM_LOCK_INIT_FAILED - ret];
            }
        }
        struct timeval e;
        gettimeofday(&e, NULL);
        printf("f=%d, use_us=%jd\n", f, (int64_t)e.tv_sec*1000000 - (int64_t)b.tv_sec*1000000 + (e.tv_usec - b.tv_usec));
        for(size_t i = 0; i < EC_QUE_SHM_LOCK_INIT_FAILED - EC_QUE_SHM_EMPTY + 1; ++i)
        {
            printf("i=%d, e=%d\n", i, fa[i]);
        }
    }
    else if(key == "loop_pop")
    {
        if(argc < 4)
        {
            help(argv);
            return -1;
        }

        int n = atol(argv[3]);

        int* fa = new int[EC_QUE_SHM_LOCK_INIT_FAILED - EC_QUE_SHM_EMPTY + 1];
        int be = n / 10 * 2;
        int f = 0;
        int64_t* r = new int64_t[n];
        struct timeval b;
        gettimeofday(&b, NULL);
        for (int i = 0; i < n; i++)
        {
            for(;;)
            {
                ret = que.Pop(v);
                if(0 == ret)
                {
                    break;
                }
                ++f;
                if(i > be && ret <= EC_QUE_SHM_LOCK_INIT_FAILED && ret >= EC_QUE_SHM_EMPTY)
                    ++fa[EC_QUE_SHM_LOCK_INIT_FAILED - ret];
            }
            r[i] = *(int*)v.c_str();
        }
        struct timeval e;
        gettimeofday(&e, NULL);
        printf("e=%d, use_us=%jd\n", f, (int64_t)e.tv_sec*1000000 - (int64_t)b.tv_sec*1000000 + (e.tv_usec - b.tv_usec));
        for(size_t i = 0; i < EC_QUE_SHM_LOCK_INIT_FAILED - EC_QUE_SHM_EMPTY + 1; ++i)
        {
            printf("i=%d, e=%d\n", i, fa[i]);
        }

        if(argc > 4)
        {
            ofstream os(argv[4], ios::trunc);
            for (int i = 0; i < n; i++)
            {
                os << r[i] << endl;
            }
        }
    }
    else
    {
        unsigned uiUsedNum = 0;
        unsigned uiAllNum = 0;

        que.GetIndexInfo(uiUsedNum, uiAllNum);

        unsigned uiBlockDataSize = 0;
        unsigned uiAllBlockNum = 0;
        unsigned uiUsedBlockNum = 0;

        que.GetValueBlockArrayInfo(uiBlockDataSize, uiAllBlockNum, uiUsedBlockNum);

        printf("uiUsedNum=%u, uiAllNum=%u\n", uiUsedNum, uiAllNum);
        printf("uiBlockDataSize=%u, uiAllBlockNum=%u, uiUsedBlockNum=%u\n",
               uiBlockDataSize, uiAllBlockNum, uiUsedBlockNum);
    }

    return 0;
}

