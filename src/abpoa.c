#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "abpoa.h"
#include "abpoa_graph.h"
#include "abpoa_align.h"
#include "abpoa_graph_visual.h"
#include "align.h"
#include "simd_instruction.h"
#include "kseq.h"
#include "utils.h"

KSEQ_INIT(gzFile, gzread)

char PROG[20] = "abPOA";

abpoa_para_t *abpoa_init_para(void) {
    abpoa_para_t *abpt = (abpoa_para_t*)_err_malloc(sizeof(abpoa_para_t));
    abpt->align_mode = ABPOA_GLOBAL_MODE;
    abpt->zdrop = -1;     // disable zdrop
    abpt->end_bonus = -1; // disable end bouns
    abpt->bw = -1;        // disable bandwidth
    abpt->use_ada = 0;    // use adaptive band
    abpt->ret_cigar = 1;  // return cigar
    abpt->out_msa = 0;    // output msa
    abpt->out_cons = 0;   // output consensus sequence in msa
    abpt->cons_agrm = 0;   // consensus calling algorithm 
    abpt->out_pog= 0; // generate partial order graph

    // number of residue types
    abpt->m = 5; // nucleotides
    abpt->mat = NULL; // TODO score matrix for aa

    // score matrix
    abpt->match = ABPOA_MATCH;
    abpt->mismatch = ABPOA_MISMATCH;
    abpt->gap_open = ABPOA_GAP_OPEN;
    abpt->gap_ext = ABPOA_GAP_EXT;

    abpt->simd_flag = simd_check();

    return abpt;
}

void abpoa_free_para(abpoa_para_t *abpt) {
    if (abpt->mat != NULL) free(abpt->mat);
    free(abpt);
}

const struct option abpoa_long_opt [] = {
    { "align-mode", 1, NULL, 'm' },
    { "ada-band", 0, NULL, 'a' },
    { "bandwidth", 1, NULL, 'w' },
    { "zdrop", 1, NULL, 'z' },
    { "end-bouns", 1, NULL, 'b' },
    { "match", 1, NULL, 'M' },
    { "mismatch", 1, NULL, 'x' },
    { "gap-open", 1, NULL, 'o' },
    { "gap-ext", 1, NULL, 'e' },
    { "out-cons", 0, NULL, 'c' },
    { "out-msa", 0, NULL, 's' },
    { "cons-agrm", 1, NULL, 'C' },

    { 0, 0, 0, 0}
};
int abpoa_usage(void)
{
    err_printf("\n");
    err_printf("Usage:   %s [option] <in.fa/fq> > msa.out\n\n", PROG);
    err_printf("Options:\n\n");
    err_printf("         -m --align-mode  [INT]    align mode. [0]\n");
    err_printf("                                     0: glogal\n");
    err_printf("                                     1: local\n");
    err_printf("                                     2: extension\n");
    err_printf("                                     3: semi-global\n\n");

    err_printf("         -M --match       [INT]    match score. [%d]\n", ABPOA_MATCH);
    err_printf("         -x --mismatch    [INT]    mismatch penalty. [%d]\n", ABPOA_MISMATCH);
    err_printf("         -o --gap-open    [INT]    gap open penalty. [%d]\n", ABPOA_GAP_OPEN);
    err_printf("         -e --gap-ext     [INT]    gap extension penalty [%d]\n", ABPOA_GAP_EXT);
    err_printf("         -w --bandwidth   [INT]    bandwidth used in alignment. [-1]\n");
    err_printf("         -a --ada-band             adaptively update band during alignment. [False]\n");
    err_printf("         -z --zdrop       [INT]    Z-drop score. [-1]\n");
    err_printf("         -e --end-bonus   [INT]    end bonus score. [-1]\n\n");

    err_printf("         -c --out-cons             output consensus sequence. [False]\n");
    err_printf("         -s --out-msa              output multiple sequence alignment in pir format. [False]\n");
    err_printf("         -C --cons-agrm   [INT]    algorithm for consensus calling. [0]\n");
    err_printf("                                      0: heavest bundling\n");
    err_printf("                                      1: minimum flow\n\n");
    err_printf("         -g --out-pog              generate visualized partial-order graph. [False]\n");

    err_printf("\n");
    return 1;
}
void cons_to_seq_score(int cons_l, uint8_t *cons_seq, int seq_n, char (*seq)[100], abpoa_para_t *abpt) {
    int i, j;
    abpt->bw = -1; // disable band width
    printf("cons_to_seq:\n");
    uint8_t *bseq = (uint8_t*)_err_malloc(100 * sizeof(uint8_t)); int bseq_m = 100;
    // progressively partial order alignment
    abpoa_t *ab = abpoa_init();
    for (i = 0; i < seq_n; ++i) {
        abpoa_reset_graph(ab, 100, abpt);
        int seq_l = strlen(seq[i]);
        if (seq_l > bseq_m) {
            bseq_m = seq_l;
            bseq = (uint8_t*)_err_realloc(bseq, bseq_m * sizeof(uint8_t));
        }
        abpoa_cigar_t *abpoa_cigar=0; int n_cigar=0;
        abpoa_align_sequence_with_graph(ab, cons_seq, cons_l, abpt, &n_cigar, &abpoa_cigar); 
        abpoa_add_graph_alignment(ab->abg, abpt, cons_seq, cons_l, n_cigar, abpoa_cigar, NULL, NULL); if (n_cigar) free(abpoa_cigar);

        for (j = 0; j < seq_l; ++j) bseq[j] = nst_nt4_table[(int)(seq[i][j])];
        abpoa_align_sequence_with_graph(ab, bseq, seq_l, abpt, &n_cigar, &abpoa_cigar); if (n_cigar) free(abpoa_cigar);
    }

    int rest_n = 0, rest_l, rest_i;
    char rest[100][100] = { 
        "GTCGTAAAGAACGTAGGTCGCCCGTCCGTAATCTGTCGGATCACCGGAAAGATGACGACCCGTAAAGTGATAATGATCAT",
        "CATAAAGAACGTAGGTCGCCGTGAGTCCGTAATCCGTACGGATTCACCGGAATGGCGTAGTTACCCGATAAAGTGATAATACAT",
    };
    uint8_t rest_seq[100];
    for (rest_i = 0; rest_i < rest_n; ++rest_i) {
        printf("REST# %d: cons_to_seq:\n", rest_i);
        rest_l = strlen(rest[rest_i]);
        for (i = 0; i < rest_l; ++i) rest_seq[i] = nst_nt4_table[(int)(rest[rest_i][i])];
        // progressively partial order alignment
        for (i = 0; i < seq_n; ++i) {
            abpoa_reset_graph(ab, 100, abpt);
            int seq_l = strlen(seq[i]);
            if (seq_l > bseq_m) {
                bseq_m = seq_l;
                bseq = (uint8_t*)_err_realloc(bseq, bseq_m * sizeof(uint8_t));
            }
            abpoa_cigar_t *abpoa_cigar=0; int n_cigar=0;
            abpoa_align_sequence_with_graph(ab, rest_seq, rest_l, abpt, &n_cigar, &abpoa_cigar); 
            abpoa_add_graph_alignment(ab->abg, abpt, rest_seq, rest_l, n_cigar, abpoa_cigar, NULL, NULL); if (n_cigar) free(abpoa_cigar);

            for (j = 0; j < seq_l; ++j) bseq[j] = nst_nt4_table[(int)(seq[i][j])];
            abpoa_align_sequence_with_graph(ab, bseq, seq_l, abpt, &n_cigar, &abpoa_cigar); if (n_cigar) free(abpoa_cigar);
        }
    }
    abpoa_free(ab); free(bseq);
}

int abpoa_read_seq(kseq_t *read_seq, int chunk_read_n)
{
    kseq_t *s = read_seq;
    int n = 0;
    while (kseq_read(s+n) >= 0) {
        n++;
        if (n >= chunk_read_n) break;
    }
    return n;
}

int abpoa_main(const char *list_fn, abpoa_para_t *abpt){
    FILE *list_fp = fopen(list_fn, "r"); char read_fn[1024];
    kseq_t *read_seq = (kseq_t*)calloc(CHUNK_READ_N, sizeof(kseq_t));
    int bseq_m = 1000; uint8_t *bseq = (uint8_t*)_err_malloc(bseq_m * sizeof(uint8_t));
    int nseqs_m, *seq_m, *seq_node_ids_l, **seq_node_ids; // for msa
    int i, j, n_seqs, tot_n;
    if (abpt->out_msa) {
        nseqs_m = CHUNK_READ_N;
        seq_node_ids_l = (int*)_err_malloc(nseqs_m * sizeof(int));
        seq_node_ids = (int**)_err_malloc(nseqs_m * sizeof(int*));
        seq_m = (int*)_err_calloc(nseqs_m, sizeof(int));
        for (i = 0; i < nseqs_m; ++i) {
            seq_m[i] = bseq_m;
            seq_node_ids[i] = (int*)_err_malloc(bseq_m * sizeof(int));
        }
    }
    // TODO abpoa_init for each input file ???
    abpoa_t *ab = abpoa_init();
    while (fgets(read_fn, sizeof(read_fn), list_fp)) {
        read_fn[strlen(read_fn)-1] = '\0';
        gzFile readfp = xzopen(read_fn, "r"); kstream_t *fs = ks_init(readfp);
        for (i = 0; i < CHUNK_READ_N; ++i) read_seq[i].f = fs;
        // progressively partial order alignment
        n_seqs = tot_n = 0;
        // reset graph for a new input file
        abpoa_reset_graph(ab, bseq_m, abpt);
        while ((n_seqs = abpoa_read_seq(read_seq, CHUNK_READ_N)) != 0) {
            if (abpt->out_msa) {
                if (tot_n + n_seqs > nseqs_m) {
                    int m = MAX_OF_TWO(tot_n + n_seqs, nseqs_m << 1);
                    seq_node_ids_l = (int*)_err_realloc(seq_node_ids_l, m * sizeof(int));
                    seq_node_ids = (int**)_err_realloc(seq_node_ids, m * sizeof(int*));
                    seq_m = (int*)_err_realloc(seq_m, m * sizeof(int));
                    for (i = nseqs_m; i < m; ++i) {
                        seq_m[i] = bseq_m;
                        seq_node_ids[i] = (int*)_err_malloc(bseq_m * sizeof(int));
                    }
                    nseqs_m = m;
                }
            }
            for (i = 0; i < n_seqs; ++i) {
                kseq_t *seq = read_seq + i;
                int seq_l = seq->seq.l;
                char *seq1 = seq->seq.s;
#ifdef __DEBUG__
                printf("seq(%d): %s\n", seq_l, seq1);
#endif
                if (seq_l > bseq_m) {
                    bseq_m = MAX_OF_TWO(seq_l+1, bseq_m * 2);
                    bseq = (uint8_t*)_err_realloc(bseq, bseq_m * sizeof(uint8_t));
                }
                for (j = 0; j < seq_l; ++j) bseq[j] = nst_nt4_table[(int)(seq1[j])];
                abpoa_cigar_t *abpoa_cigar=0; int n_cigar=0;
                abpoa_align_sequence_with_graph(ab, bseq, seq_l, abpt, &n_cigar, &abpoa_cigar);
                if (abpt->out_msa) {
                    if (seq_l > seq_m[tot_n+i]) {
                        seq_m[tot_n+i] = MAX_OF_TWO(seq_l, seq_m[tot_n+i] * 2);
                        seq_node_ids[tot_n+i] = (int*)_err_realloc(seq_node_ids[tot_n+i], seq_m[tot_n+i] * sizeof(int));
                    }
                    seq_node_ids_l[tot_n+i] = 0;
                    abpoa_add_graph_alignment(ab->abg, abpt, bseq, seq_l, n_cigar, abpoa_cigar, seq_node_ids[tot_n+i], seq_node_ids_l+tot_n+i);
                } else abpoa_add_graph_alignment(ab->abg, abpt, bseq, seq_l, n_cigar, abpoa_cigar, NULL, NULL); 
                if (n_cigar) free(abpoa_cigar);
                if (abpt->out_pog) {
                    char abpoa_dot_fn[100]; sprintf(abpoa_dot_fn, "./dot_plot/abpoa_%d.dot", i);
                    abpoa_graph_visual(ab->abg, abpoa_dot_fn);
                }
            }
            tot_n += n_seqs;
        }
        // generate consensus from graph
        if (abpt->out_cons) 
            abpoa_generate_consensus(ab->abg, abpt->cons_agrm, stdout);
        // generate multiple sequence alignment
        if (abpt->out_msa) 
            abpoa_generate_multiple_sequence_alingment(ab->abg, seq_node_ids, seq_node_ids_l, tot_n, abpt->out_cons, stdout);
        ks_destroy(fs); gzclose(readfp); 
    }
    if (abpt->out_msa) { 
        for (i = 0; i < nseqs_m; ++i) free(seq_node_ids[i]); 
        free(seq_node_ids); free(seq_node_ids_l); free(seq_m);
    } free(bseq);
    for (i = 0; i < CHUNK_READ_N; ++i) { free((read_seq+i)->name.s); free((read_seq+i)->comment.s); free((read_seq+i)->seq.s); free((read_seq+i)->qual.s); } free(read_seq); 
    fclose(list_fp); abpoa_free(ab);
    return 0;
}
/*int abpoa_main(const char *read_fn, abpoa_para_t *abpt){
    int i, j, n_seqs=0, tot_n=0;
    abpoa_graph_t *graph = abpoa_init_graph(1);
    gzFile readfp = xzopen(read_fn, "r"); kstream_t *fs = ks_init(readfp);
    kseq_t *read_seq = (kseq_t*)calloc(CHUNK_READ_N, sizeof(kseq_t));
    for (i = 0; i < CHUNK_READ_N; ++i) read_seq[i].f = fs;

    int **seq_node_ids = (int**)_err_malloc(CHUNK_READ_N * sizeof(int*));
    int *seq_node_ids_l = (int*)_err_malloc(CHUNK_READ_N * sizeof(int));
    uint8_t *bseq = (uint8_t*)_err_malloc(100 * sizeof(uint8_t)); int bseq_m = 100;

    // progressively partial order alignment
    while ((n_seqs = abpoa_read_seq(read_seq, CHUNK_READ_N)) != 0) {
        tot_n += n_seqs;
        for (i = 0; i < n_seqs; ++i) {
            kseq_t *seq = read_seq + i;
            int seq_l = seq->seq.l;
            char *seq1 = seq->seq.s;
#ifdef __DEBUG__
            printf("seq(%d): %s\n", seq_l, seq1);
#endif
            if (seq_l > bseq_m) {
                bseq_m = seq_l;
                bseq = (uint8_t*)_err_realloc(bseq, bseq_m * sizeof(uint8_t));
            }
            seq_node_ids[i] = (int*)_err_malloc(seq_l * sizeof(int));
            for (j = 0; j < seq_l; ++j) bseq[j] = nst_nt4_table[(int)(seq1[j])];
            abpoa_cigar_t *abpoa_cigar=0; int n_cigar=0;
            abpoa_align_sequence_with_graph(graph, bseq, seq_l, abpt, &n_cigar, &abpoa_cigar);
            seq_node_ids_l[i] = 0;
            abpoa_add_graph_alignment(graph, bseq, seq_l, n_cigar, abpoa_cigar, seq_node_ids[i], seq_node_ids_l+i);
            //printf("%d %d %d\n", seq_l, graph->rank_n, seq_node_ids_l[i]);
            char abpoa_dot_fn[100]; sprintf(abpoa_dot_fn, "./dot_plot/abpoa_%d.dot", i);
            //abpoa_graph_visual(graph, abpoa_dot_fn);
            if (n_cigar) free(abpoa_cigar);
        }
    }
    // generate consensus from graph
    abpoa_generate_consensus(graph);
    printf("consensus:\n");
    for (i = 0; i < graph->cons_l; ++i) {
        printf("%c", "ACGTN"[graph->cons_seq[i]]);
    } printf("\n");
    // generate multiple sequence alignment
    abpoa_generate_multiple_sequence_alingment(graph, seq_node_ids, seq_node_ids_l, tot_n, abpt->out_cons, stdout);

    abpoa_free_graph(graph, 1); free(bseq);
    for (i = 0; i < tot_n; ++i) free(seq_node_ids[i]); free(seq_node_ids); free(seq_node_ids_l);
    for (i = 0; i < CHUNK_READ_N; ++i) {
        free((read_seq+i)->name.s); free((read_seq+i)->comment.s); free((read_seq+i)->seq.s); free((read_seq+i)->qual.s);
    } free(read_seq); ks_destroy(fs); gzclose(readfp); 

    return 0;
}*/
// TODO multi-thread
int main(int argc, char **argv) {
    int c; abpoa_para_t *abpt = abpoa_init_para();
    while ((c = getopt_long(argc, argv, "m:aw:z:b:M:x:o:e:csC:g", abpoa_long_opt, NULL)) >= 0) {
        switch(c)
        {
            case 'm': abpt->align_mode=atoi(optarg); break;
            case 'a': abpt->use_ada = 1; break;
            case 'w': abpt->bw = atoi(optarg); break;
            case 'z': abpt->zdrop = atoi(optarg); break;
            case 'b': abpt->end_bonus= atoi(optarg); break;
            case 'M': abpt->match = atoi(optarg); break;
            case 'x': abpt->mismatch = atoi(optarg); break;
            case 'o': abpt->gap_open = atoi(optarg); break;
            case 'e': abpt->gap_ext = atoi(optarg); break;
            case 'c': abpt->out_cons = 1; break;
            case 's': abpt->out_msa = 1; break;
            case 'C': abpt->cons_agrm = atoi(optarg); break;
            case 'g': abpt->out_pog= 1; break;
            default:
                      err_printf("Error: unknown option: %s.\n", optarg);
                      return abpoa_usage();
                      break;
        }
    }

    if (argc - optind != 1) return abpoa_usage();

    abpt->mat = (int*)_err_malloc(abpt->m * abpt->m * sizeof(int));
    gen_simple_mat(abpt->m, abpt->mat, abpt->match, abpt->mismatch);
    abpoa_main(argv[optind], abpt);
    abpoa_free_para(abpt);

    return 0;
}