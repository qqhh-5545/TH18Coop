#ifndef TH18_EAGLER_CORE_H
#define TH18_EAGLER_CORE_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *ec_create(unsigned players,unsigned role,unsigned delay,unsigned window,uint64_t session);
void *ec_create_delay(unsigned players,unsigned role,unsigned delay,uint64_t session);
void ec_destroy(void *handle);
int ec_local(void *handle,uint32_t capture,uint32_t keys);
int ec_has_local(void *handle,uint32_t capture);
int ec_prepare(void *handle,uint32_t frame,uint32_t keys[3],unsigned *predicted);
int ec_mark(void *handle,uint32_t frame);
int ec_rewind(void *handle,uint32_t frame);
uint32_t ec_rollback(void *handle);
uint32_t ec_confirmed(void *handle);
int ec_inputs(void *handle,uint32_t frame,uint32_t keys[3]);
int ec_encode(void *handle,unsigned peer,uint32_t latest,uint32_t seq,uint8_t *bytes,size_t capacity,size_t *size);
int ec_receive(void *handle,unsigned sender,const uint8_t *bytes,size_t size);
#ifdef __cplusplus
}
#endif
#endif
