#include "randHelper.h"

int32_t NumberHelper::ARandRange(const int32_t a, const int32_t b, unsigned int *seed)
{   
    if(a >= b)
    {   
        return a;
    }   
        
    int32_t num_min = a;
    int32_t num_max = b + 1;
    
    int32_t r = ARand(seed);
    
    int32_t result = (num_min + (int32_t)((num_max - num_min) * (r / (RAND_MAX + 1.0))));
        
    //int result = arand() % (b - a + 1) + a;
        
    return result;
}
