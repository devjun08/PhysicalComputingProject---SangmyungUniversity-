#include "protocol.h"

/* CRC-8 (poly 0x07, init 0x00, 반사 없음). MATLAB ls_crc8.m과 동일해야 함 */
uint8_t Proto_CRC8(const uint8_t *data, uint32_t len)
{
    uint8_t crc = 0x00;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80u) crc = (uint8_t)((crc << 1) ^ 0x07u);
            else             crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

uint32_t Proto_BuildFrame(const ProtoStatus *st, uint8_t out[PROTO_FRAME_LEN])
{
    uint32_t i = 0;

    out[i++] = PROTO_HEAD;
    out[i++] = st->seq;
    out[i++] = st->state;
    out[i++] = st->hit_cnt;
    out[i++] = st->zone;
    out[i++] = (uint8_t)(st->reaction_ms >> 8);   /* 상위 바이트 먼저 */
    out[i++] = (uint8_t)(st->reaction_ms);
    for (uint32_t ch = 0; ch < PROTO_NUM_CH; ch++) {
        out[i++] = (uint8_t)(st->adc[ch] >> 8);
        out[i++] = (uint8_t)(st->adc[ch]);
    }
    out[i] = Proto_CRC8(&out[1], i - 1);           /* idx 1~24 */
    i++;
    out[i++] = PROTO_TAIL;

    return i;   /* == PROTO_FRAME_LEN */
}
