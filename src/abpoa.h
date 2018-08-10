#ifndef ABPOA_H
#define ABPOA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // score matrix
    int m; int *mat;
    int match, mismatch, gap_open, gap_ext; int inf_min;
    int bw; // band width
    int zdrop, end_bonus; // from minimap2
    // alignment mode
    uint8_t align_mode:2, use_ada:1, ret_cigar:1, out_msa:1, out_cons:1, cons_agrm:1, out_pog; // mode: 0: global, 1: local, 2: extend
    // available SIMD instruction
    int simd_flag;
} abpoa_para_t;

abpoa_para_t *abpoa_para_init(void);
void abpoa_para_free(abpoa_para_t *abpt);

int abpoa_main(const char *read_fn, abpoa_para_t *abpt);

#ifdef __cplusplus
}
#endif

#endif