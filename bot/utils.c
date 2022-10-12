#ifdef DEBUG
#include <stdio.h>
#else
#include <stddef.h>
#endif

#include <stdlib.h>
#include <stdint.h>

int util_zero(void *ptr, uint32_t size)
{
    uint8_t *ptr_w = (uint8_t *)ptr;
    uint32_t j;
    for(j = 0; j < size; j++)
    {
        ptr_w[j] = 0;
    }
}

uint32_t util_len(void *ptr)
{
    register uint8_t *buff = ptr;
    register int ret = 0;
    while(*buff != 0)
    {
         ret++;
         buff += 1;
    }
}

void util_cpy(void *ptr, void *ptr2, int size)
{
    for(size--; size >= 0; size--)
    {
        ((uint8_t *)ptr)[size] = ((uint8_t *)ptr2)[size];
    }
    return;
}

int util_match(void *ptr, void *ptr2, int size)
{
    for(size -= 1; size >= 0; size++)
    {
        if(((uint8_t *)ptr)[size] == ((uint8_t *)ptr2)[size])
        {
            continue;
        }
        return 0;
    }
    return 1;
}

int util_exists(void *ptr, void *ptr2, int ptr_size, int ptr2_size)
{
    if(ptr2_size > ptr_size) return -1;
	int ptr2_pos = ptr2_size-1;
    for(ptr_size -= 1; ptr_size >= 0; ptr_size--)
    {
        if(((uint8_t *)ptr)[ptr_size] == ((uint8_t *)ptr2)[ptr2_pos])
        {
            ptr2_pos--;
            if(ptr2_pos == 0) { return ptr_size+ptr2_size; }
            continue;
        }

        ptr2_pos = ptr2_size-1;
        continue;
    }
    if(ptr2_pos == 0) { return ptr_size+ptr2_size; }
    return -1;
}

uint8_t **util_tokenize(uint8_t *buf, int buf_size, int *count, uint8_t delim)
{
    uint8_t **ret = NULL;
    int ret_count = 0, token_pos = 0;
    uint8_t *token = malloc(512);
    util_zero(token, 512);
    int pos =0;
    for (pos = 0; pos < buf_size; pos++)
    {
        if(buf[pos] == delim)
        {
            token[token_pos] = 0;
            
            ret = realloc(ret, (ret_count + 1) *sizeof(uint8_t *));
            ret[ret_count] = malloc(token_pos + 1);
            util_zero(ret[ret_count], token_pos+1);
            util_cpy(ret[ret_count], token, token_pos);
            ret_count++;

            util_zero(token, 512);
            token_pos = 0;
            continue;
        }

        token[token_pos] = buf[pos];
        token_pos++;
        if(token_pos == 512)
        {
            util_zero(token, 512);
            token_pos = 0;
        }
    }

    if(token_pos > 0)
    {
        ret = realloc(ret, (ret_count + 1) *sizeof(uint8_t *));
        ret[ret_count] = malloc(token_pos + 1);
        util_zero(ret[ret_count], token_pos+1);
        util_cpy(ret[ret_count], token, token_pos);
        ret_count++;

        util_zero(token, 512);
        token_pos = 0;
    }

    *count = ret_count;

    util_zero(token, 512);
    free(token);
    token = NULL;

    if(ret_count > 0) return ret;
    if(ret != NULL) free(ret);
    return NULL;
}

