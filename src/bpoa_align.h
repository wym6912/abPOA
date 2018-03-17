#ifndef POA_ALIGN_H
#define POA_ALIGN_H

#include "bpoa.h"
#include "bpoa_graph.h"

// SPOA
#define POA_MATCH  5
#define POA_MISMATCH  4
#define POA_GAP_OPEN  0
#define POA_GAP_EXT  8

//#define POA_MATCH  1
//#define POA_MISMATCH  3
//#define POA_GAP_OPEN  5
//#define POA_GAP_EXT  2

#define POA_GLOBAL_FLAG 0
#define POA_LOCAL_FLAG 1
#define POA_EXTEND_FLAG 2

#define POA_CIGAR_STR "MIDXSH"
#define POA_CMATCH     0
#define POA_CINS       1
#define POA_CDEL       2
#define POA_CDIFF      3
#define POA_CSOFT_CLIP 4
#define POA_CHARD_CLIP 5

#define HOP_OFF_SET   0
#define EOP_OFF_SET   2
#define FOP_OFF_SET   4

#define INF_8_MIN  -0x80
#define INF_8_MAX  0x7f
#define INF_16_MIN -0x8000
#define INF_16_MAX 0x7fff
#define INF_32_MIN -0x80000000
#define INF_32_MAX 0x7fffffff
#define INF_64_MIN -0x8000000000000000
#define INF_64_MAX 0x7fffffffffffffff

// calculate band range for each row:
// have: min_rank, max_rank, min_remain, max_remain
// then: min_len = min_rank + min_remain, max_len = min_rank + max_remain
// range: (min_of_two(min_rank, min_rank+qlen-max_len), max_of_two(min_rank+qlen-min_len, max_rank))
// with w: (min-w, max+w)
#define GET_DP_BEGIN(graph, w, i) MAX_OF_TWO(0, MIN_OF_TWO(bpoa_graph_index_to_min_rank(graph, i), qlen - bpoa_graph_index_to_max_remain(graph, i))-w)
#define GET_DP_END(graph, w, i) MIN_OF_TWO(qlen, MAX_OF_TWO(bpoa_graph_index_to_max_rank(graph, i), qlen - bpoa_graph_index_to_min_remain(graph, i))+w)

#ifdef __cplusplus
extern "C" {
#endif

void gen_simple_mat(int m, int *mat, int match, int mismatch);
//bpoa_graph_cigar_t *bpoa_init_graph_cigar(int n);
//void bpoa_free_graph_cigar(bpoa_graph_cigar_t *bpoa_cigar, int n);

// TODO splice mode
/* banded global partial order graph alignment */
int bpoa_banded_global_align_sequence_with_graph(bpoa_graph_t *graph, uint8_t *query, int qlen, bpoa_para_t *bpt, int *n_cigar, bpoa_cigar_t **graph_cigar);

/* Adaptive banded global partial order graph alignment */
int bpoa_ada_forefront_global_align_sequence_with_graph(bpoa_graph_t *graph, uint8_t *query, int qlen, bpoa_para_t *bpt, int *n_cigar, bpoa_cigar_t **graph_cigar);


/* Banded global partial order graph alignment */
int bpoa_global_align_sequence_with_graph(bpoa_graph_t *graph, uint8_t *query, int qlen, bpoa_para_t *bpt, int *n_cigar, bpoa_cigar_t **graph_cigar);

static inline bpoa_cigar_t *bpoa_push_cigar(int *n_cigar, int *m_cigar, bpoa_cigar_t *cigar, int op, int len, int32_t node_id, int32_t query_id) {
    bpoa_cigar_t l = len;
    if (*n_cigar == 0 || (op != POA_CINS && op != POA_CSOFT_CLIP && op != POA_CHARD_CLIP) || op != (cigar[(*n_cigar)-1] & 0xf)) {
        if (*n_cigar == *m_cigar) {
            *m_cigar = *m_cigar? (*m_cigar)<<1 : 4;
            cigar = (bpoa_cigar_t*)_err_realloc(cigar, (*m_cigar) * sizeof(bpoa_cigar_t));
        }
        bpoa_cigar_t n_id = node_id, q_id = query_id;
        if (op == POA_CMATCH || op == POA_CDIFF) 
            cigar[(*n_cigar)++] = n_id << 34 | q_id << 4 | op;
        else if (op == POA_CINS || op == POA_CSOFT_CLIP || op == POA_CHARD_CLIP) 
            cigar[(*n_cigar)++] = q_id << 34 | l << 4 | op;
        else if (op == POA_CDEL)
            cigar[(*n_cigar)++] = n_id << 34 | l << 4 | op;
        else
            err_fatal(__func__, "Unknown cigar operation: %s\n", op);
    } else cigar[(*n_cigar)-1] += l << 4;

    return cigar;
}

static inline bpoa_cigar_t *bpoa_reverse_cigar(int n_cigar, bpoa_cigar_t *cigar) {
    int i; bpoa_cigar_t tmp;
    for (i = 0; i < n_cigar >> 1; ++i) {
        tmp = cigar[i];
        cigar[i] = cigar[n_cigar-1-i];
        cigar[n_cigar-1-i] = tmp;
    }
    return cigar;
}

static inline void bpoa_print_cigar(int n_cigar, bpoa_cigar_t *cigar, bpoa_graph_t *graph) {
    int i, op, len, node_id, query_id, index_i;
    int n[6] = {0, 0, 0, 0, 0, 0};
    for (i = 0; i < n_cigar; ++i) {
        op = cigar[i] & 0xf; node_id = (int)(cigar[i] >> 34); 
        len = query_id = (int)(cigar[i] >> 4) & 0x3fffffff;
        if (op == POA_CMATCH || op == POA_CDIFF) {
            index_i = bpoa_graph_node_id_to_index(graph, node_id);
            printf("1%c:%d,%d\t", POA_CIGAR_STR[op], index_i, query_id);
            n[op] += 1;
        } else if (op == POA_CDEL) {
            index_i = bpoa_graph_node_id_to_index(graph, node_id);
            printf("%d%c:%d\t", len, POA_CIGAR_STR[op], index_i);
            n[op] += len;
        } else if (op == POA_CINS || op == POA_CSOFT_CLIP || op == POA_CHARD_CLIP) { 
            printf("%d%c:%d\t", len, POA_CIGAR_STR[op], node_id);
            n[op] += len;
        } else {
            err_fatal(__func__, "Unknown cigar operation: %s\n", op);
        }
    } printf("\n");
    for (i = 0; i < 6; ++i)
        printf("%d%c ", n[i], POA_CIGAR_STR[i]);
    printf("\n");
}

// type of score matrix (DP_H, DP_E): 8/16/32
static inline void bpoa_backtrack(int *DP_H, int *DP_E, int matrix_col_n, int8_t match, int8_t mis, int8_t gap_e, int **pre_index, int *pre_n, uint8_t *backtrack_z, int best_i, int best_j, int z_col_n, bpoa_graph_t *graph, uint8_t *query, int *n_cigar, bpoa_cigar_t **graph_cigar) {
    int i, j, k, pre_i;
    if (n_cigar && graph_cigar) {
        int n_c = 0, s, m_c = 0, id, which, last_which;
        int op_shift[3] = {HOP_OFF_SET, EOP_OFF_SET, FOP_OFF_SET};
        uint8_t d;
        bpoa_cigar_t *cigar = 0;
        i = best_i, j = best_j, id = bpoa_graph_index_to_node_id(graph, i), last_which = 0;
        while (i > 0 && j > 0) {
            d = backtrack_z[(long)(i-1) * z_col_n + j-1];
            which = (d >> op_shift[last_which]) & 3;
            if (which == 0) { // match
                cigar = bpoa_push_cigar(&n_c, &m_c, cigar, POA_CMATCH, 1, id, j-1);
                s = graph->node[id].base == query[j-1] ? match : -mis;

                for (k = 0; k < pre_n[i]; ++k) {
                    pre_i = pre_index[i][k];
                    if (DP_H[pre_i * matrix_col_n + j-1] + s == DP_H[i * matrix_col_n + j]) {
                        i = pre_i;
                        break;
                    }
                }
                id = bpoa_graph_index_to_node_id(graph, i);
                j--; last_which = which;
            } else if (which == 1) { // deletion
                cigar = bpoa_push_cigar(&n_c, &m_c, cigar, POA_CDEL, 1, id, j-1);
                if (last_which == 1) {
                    for (k = 0; k < pre_n[i]; ++k) {
                        pre_i = pre_index[i][k];
                        if (DP_E[pre_i * matrix_col_n + j] - gap_e == DP_E[i * matrix_col_n + j]) {
                            i = pre_i;
                            break;
                        }
                    }
                } else if (last_which == 0) {
                    for (k = 0; k < pre_n[i]; ++k) {
                        pre_i = pre_index[i][k];
                        if (DP_E[pre_i * matrix_col_n + j] == DP_H[i * matrix_col_n + j]) {
                            i = pre_i;
                            break;
                        }
                    }
                } else {
                    printf("\nunexpected cigar op.\n");
                }
                id = bpoa_graph_index_to_node_id(graph, i);
                last_which = which;
            } else { // insertion
                cigar = bpoa_push_cigar(&n_c, &m_c, cigar, POA_CINS, 1, id, j-1);
                j--; last_which = which;
            }
        }
        if (j > 0) cigar = bpoa_push_cigar(&n_c, &m_c, cigar, POA_CSOFT_CLIP, j, -1, j-1);
        // reverse cigar
        *graph_cigar = bpoa_reverse_cigar(n_c, cigar);
        *n_cigar = n_c;
#ifdef __DEBUG__
        bpoa_print_cigar(n_c, *graph_cigar, graph);
#endif
    }
}


#define _set_max_score(best_score, best_i, best_j, score, i, j) { \
    if (score > best_score) {                                     \
        best_score = score; best_i = i; best_j = j;               \
    }                                                             \
}

#ifdef __cplusplus
}
#endif

#endif