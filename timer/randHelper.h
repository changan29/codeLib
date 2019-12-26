#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <stdint.h> 
#include <iostream>
#include <vector>
using namespace std;

template <class T> struct prop_extractor
{
    uint32_t operator()(const T& x) const { return x.prop; }
};

class NumberHelper
{
public:
    /* 用来代替rand的函数，用rand_r实现。*/
    static inline int32_t ARand(unsigned int* seed = NULL)
    {
        if (NULL == seed)
        {
            static unsigned int s__ = time(NULL) + getpid();
            return rand_r(&s__);
        }
        else
        {
            return rand_r(seed);
        }
    }

    /* random_shuffle 专用*/
    static inline int32_t arandom_shuffle_generator(int32_t i)
    {
        return ARand() % i;
    }
    
    /* 产生[a, b]之间的随机数，如果a>=b，直接返回a
     */
    static int32_t ARandRange(const int32_t a, int32_t b, unsigned int* seed = NULL);
    
    //by kevin 产生[0-1)之前的随机数, 注意rand()函数的区间是[0,RAND_MAX], 所以要+1
    //测试了一下tr1::random中的一些做法，还是自己实现更高效。
    static inline double Random01(unsigned int* seed = NULL)
    {
        return  ARand(seed) / (double(RAND_MAX) + 1);
    }

    // N选1
    template< class T, class ExtractProp = prop_extractor<T>  >
        static int ProbNChooseOne(const std::vector<T> & prob)
    {
        uint32_t sum = 0;
        ExtractProp _extractor;
        for (uint32_t i = 0; i < prob.size(); i++)
        {
            sum += _extractor(prob[i]);
        }
        uint32_t c = 0;
        uint32_t r = arandom_shuffle_generator(sum);

        for (uint32_t i = 0; i < prob.size(); i++)
        {
            c += _extractor(prob[i]);
            if (c > r)
            {
                return i;
            }
        }
        return prob.size() - 1;
    }
    //    /*
    //     * 规范数值大小
    //     */
    //    template<class T>
    //    static void check_max_limit(T& a, int32_t max)
    //    {
    //        a = (a > max) ? max : a;
    //    }
};
