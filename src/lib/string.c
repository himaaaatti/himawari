#include <string.h>

#include <stdbool.h>
#include <stdint.h>

void * memset(void * s,int c, size_t count)
{
    char *xs = (char *) s;

    while (count--)
        *xs++ = c;

    return s;
}

bool itoa(
        uint64_t num,
        char* buf,
        const uint64_t decimal)
{
    const int size = 32;
    char tmp[size];
    int next_pos = 0;

    static const char* trans_table = "0123456789ABCDEF";

    for(int i=0; i<size; ++i)
    {
        int dig = num % decimal;
        if(decimal == 10){
            tmp[next_pos++] = '0' + dig;
        }
        else if(decimal == 0x10)
        {
            tmp[next_pos++] = trans_table[dig];
        }
        else {
            return false;
        }
        
        num /= decimal;
        if(num == 0) {
            break;
        }
    }

    for(int i=0; i<next_pos; ++i)
    {
        buf[i] = tmp[next_pos - i - 1];
    }
    buf[next_pos] = '\0';

    return true;
}


