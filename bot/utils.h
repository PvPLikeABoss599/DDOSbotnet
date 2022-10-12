#pragma once

void util_zero(void *ptr, int size);
uint32_t util_len(void *ptr);
void util_cpy(void *ptr, void *ptr2, int size);
int util_match(void *ptr, void *ptr2, int size);
int util_exists(void *ptr, void *ptr2, int ptr_size, int ptr2_size);
uint8_t **util_tokenize(uint8_t *buf, int buf_size, int *count, uint8_t delim);
