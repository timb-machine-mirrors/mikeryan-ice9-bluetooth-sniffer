/*
 * Copyright 2023 ICE9 Consulting LLC
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <arm_neon.h>

#include "window.h"

void window_init(window_t *w, unsigned n) {
    memset(w, 0, sizeof(*w));

    w->len = n;
    w->m = (unsigned)floor(log2(n)) + 1;
    w->n = 1 << w->m;
    w->mask = w->n - 1;

    w->num_allocated = w->n + w->len - 1;
    w->r = malloc(sizeof(int16_t) * w->num_allocated);
    w->i = malloc(sizeof(int16_t) * w->num_allocated);
}

void window_push(window_t *w, int8_t *v) {
    ++w->read_index;
    w->read_index &= w->mask;
    if (w->read_index == 0) {
        memmove(w->r, w->r + w->n, (w->len - 1) * sizeof(*w->r));
        memmove(w->i, w->i + w->n, (w->len - 1) * sizeof(*w->i));
    }
    w->r[w->read_index + w->len - 1] = (int16_t)v[0] << 8;
    w->i[w->read_index + w->len - 1] = (int16_t)v[1] << 8;
}

/*
void window_dotprod(window_t *w, int16_t *b, int16_t *out) {
    unsigned i;
    int32_t sum_real = 0, sum_imag = 0;
    for (i = 0; i < w->len; ++i) {
        sum_real += w->r[w->read_index + i] * b[i];
        sum_imag += w->r[w->read_index + i] * b[i];
    }
    out[0] = sum_real >> 16;
    out[1] = sum_imag >> 16;
}
*/

void window_dotprod(window_t *w, int16_t *b, int16_t *out) {
    unsigned i;
    int32x4_t real_acc = vdupq_n_s32(0), imag_acc = vdupq_n_s32(0);;

    for (i = 0; i < w->len; i += 4) {
        int16x4_t real_a_vec = vld1_s16(&w->r[w->read_index + i]);
        int16x4_t imag_a_vec = vld1_s16(&w->i[w->read_index + i]);
        int16x4_t b_vec = vld1_s16(&b[i]);

        int32x4_t real_a_ext = vmovl_s16(real_a_vec);
        int32x4_t imag_a_ext = vmovl_s16(imag_a_vec);
        int32x4_t b_ext = vmovl_s16(b_vec);

        real_acc = vmlaq_s32(real_acc, real_a_ext, b_ext);
        imag_acc = vmlaq_s32(imag_acc, imag_a_ext, b_ext);
    }

    int32_t real_sum = vgetq_lane_s32(real_acc, 0) + vgetq_lane_s32(real_acc, 1) +
                       vgetq_lane_s32(real_acc, 2) + vgetq_lane_s32(real_acc, 3);
    int32_t imag_sum = vgetq_lane_s32(imag_acc, 0) + vgetq_lane_s32(imag_acc, 1) +
                       vgetq_lane_s32(imag_acc, 2) + vgetq_lane_s32(imag_acc, 3);

    out[0] = real_sum >> 16;
    out[1] = imag_sum >> 16;
}