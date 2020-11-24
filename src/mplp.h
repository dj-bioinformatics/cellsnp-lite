/* Pileup and MPileup operatoins API/routine
 * Author: Xianjie Huang <hxj5@hku.hk>
 */
#ifndef CSP_MPLP_H
#define CSP_MPLP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "htslib/sam.h"
#include "htslib/kstring.h"
#include "htslib/khash.h"
#include "kvec.h"
#include "jnumeric.h"
#include "jfile.h"
#include "jmemory.h"
#include "config.h"

/*
* Pileup and MPileup API
 */
/*@abstract          Store statistics result of one read matching the query pos.
@param b             Pointer of bam1_t structure.
@param qpos          The index of the query pos in the read seq.
@param base          The base corresponding to the query pos, equal to read_seq[qpos].
@param qual          The qual corresponding to the query pos, equal to qual_seq[qpos].
@param is_refskip    0/1. if the query pos is in res-skip region.
@param is_del        0/1. if the query pos is in deletion region (also set 1 when is_refskip to be compatible with sam.c).
@param umi           Pointer to UMI tag.
@param cb            Pointer to cell barcode.
@param laln          Length of the read part that aligned to reference.

@note  1. The umi and cb in the structure would be extracted from bam1_t, so do not need to free it as the bam1_t will do that!
          Refer to bam_get_aux(), bam_aux_get() and bam_aux2Z() in sam.h.
 */
typedef struct { 
    bam1_t *b;
    int32_t qpos;
    int8_t base, qual;
    uint8_t is_refskip:1, is_del:1;
    char *umi, *cb;
    uint32_t laln;
} csp_pileup_t;

/*@abstract   Initialize the csp_pileup_t structure.
@return       Pointer to csp_pileup_t structure if success, NULL otherwise.

@note         1. Memory for bam1_t is allocated in this function.
              2. The pointer returned successfully by csp_pileup_init() should be freed
                 by csp_pileup_destroy() when no longer used.
 */
static inline csp_pileup_t* csp_pileup_init(void) {
    csp_pileup_t *p = (csp_pileup_t*) malloc(sizeof(csp_pileup_t));
    if (p) {
        if (NULL == (p->b = bam_init1())) { free(p); return NULL; }
    }
    return p;
}

static inline void csp_pileup_destroy(csp_pileup_t *p) { 
    if (p) {
        if (p->b) bam_destroy1(p->b);	
        free(p);
    } 
}

/* reset the csp_pileup_t structure without reallocating memory.
   return 0 if success, -1 otherwise. */
static inline int csp_pileup_reset(csp_pileup_t *p) {
    if (p) {
        if (p->b) { bam_destroy1(p->b); }
        memset(p, 0, sizeof(csp_pileup_t)); 
        if (NULL == (p->b = bam_init1())) { return -1; }
    } // else: do nothing.
    return 0;
}

/* only reset part of the csp_pileup_t as values of parameters in other parts will be immediately overwritten after
   calling this function. It's often called by pileup_read_with_fetch(). */
static inline void csp_pileup_reset_(csp_pileup_t *p) { }

static inline void csp_pileup_print(FILE *fp, csp_pileup_t *p) {
    fprintf(fp, "qpos = %d\n", p->qpos);
    fprintf(fp, "base = %c, qual = %d\n", p->base, p->qual);
    fprintf(fp, "is_refskip = %d, is_del = %d\n", p->is_refskip, p->is_del);
    fprintf(fp, "umi = %s, cb = %s\n", p->umi, p->cb);
    fprintf(fp, "len_aln = %d\n", p->laln);
}

/*@abstract   Pool that stores char*. The real elements in the pool are char**.
@example      Refer to the example of SZ_POOL in general_util.h.
@note         This structure is aimed to store dynamically allocated strings(char*).
 */
#define csp_pool_str_free(s) free(*(s))
#define csp_pool_str_reset(s) free(*(s))
SZ_POOL_INIT(ps, char*, csp_pool_str_free, csp_pool_str_reset)
typedef sz_pool_t(ps) csp_pool_ps_t;
#define csp_pool_ps_init() sz_pool_init(ps)
#define csp_pool_ps_destroy(p) sz_pool_destroy(ps, p)
#define csp_pool_ps_get(p) sz_pool_get(ps, p)
#define csp_pool_ps_reset(p) sz_pool_reset(ps, p)

/*@abstract    This structure stores stat info of one read of one UMI group for certain query pos.
@param base    The base for the query pos in the read of the UMI gruop.
               A 4-bit integer returned by bam_seqi(), which is related to bam_nt16_table(now called seq_nt16_str).
@param qual    Qual value for the query pos in the read of the UMI gruop. The value is extracted by calling bam_get_qual() and 
               could be translated to qual char by plusing 33.
 */
typedef struct {
    int8_t base, qual;
} csp_umi_unit_t;

static inline csp_umi_unit_t* csp_umi_unit_init(void) {
    csp_umi_unit_t *p = (csp_umi_unit_t*) calloc(1, sizeof(csp_umi_unit_t));
    return p;   /* will set values just after this function is called so no need to set init values here. */
}
#define csp_umi_unit_reset(p)
static inline void csp_umi_unit_destroy(csp_umi_unit_t *p) { free(p); }

/*@abstract   Pool that stores csp_umi_unit_t structures. The real elements in the pool are pointers to the csp_umi_unit_t structures.
              The pool is aimed to save the overhead of reallocating memories for csp_umi_unit_t structures.
@example      Refer to the example of SZ_POOL in general_util.h.
 */
SZ_POOL_INIT(uu, csp_umi_unit_t, csp_umi_unit_destroy, csp_umi_unit_reset)
typedef sz_pool_t(uu) csp_pool_uu_t;
#define csp_pool_uu_init() sz_pool_init(uu)
#define csp_pool_uu_destroy(p) sz_pool_destroy(uu, p)
#define csp_pool_uu_get(p) sz_pool_get(uu, p)
#define csp_pool_uu_reset(p) sz_pool_reset(uu, p)

/* Struct csp_list_uu_t APIs 
@abstract  The structure stores stat info of all reads of one UMI group for certain query pos.
@param v   Pointer to the csp_list_uu_t structure [csp_list_uu_t*].

@note      1. The elements in the list are pointers of csp_umi_unit_t, resetting the list only sets
              the list's internal var n, which stands for the next available pos in the list, to be 0.
              After calling reset function, the values (i.e. pointers of csp_umi_unit_t from csp_pool_uu) 
              newly pushed into the list would overwrite the original ones, which will not cause memory error. 
           2. The parameter in csp_list_uu_xxx functions is pointer of csp_list_uu_t.

@example   Refer to the example in kvec.h, but the parameter in csp_list_uu_xxx functions is pointer, 
           which is different from kv_xxx functions.
 */
typedef kvec_t(csp_umi_unit_t*) csp_list_uu_t;
static inline csp_list_uu_t* csp_list_uu_init(void) {
    csp_list_uu_t *v = (csp_list_uu_t*) malloc(sizeof(csp_list_uu_t));
    if (v) { kv_init(*v); }
    return v;
}
#define csp_list_uu_resize(v, size) kv_resize(csp_umi_unit_t*, *(v), size)
#define csp_list_uu_push(v, x) kv_push(csp_umi_unit_t*, *(v), x)
#define csp_list_uu_A(v, i) kv_A(*(v), i)
#define csp_list_uu_size(v) kv_size(*(v))
#define csp_list_uu_max(v) kv_max(*(v))
#define csp_list_uu_destroy(v) kv_destroy(*(v))
#define csp_list_uu_reset(v) ((v)->n = 0)

/*@abstract   Pool that stores csp_list_uu_t structures. The real elements in the pool are pointers to the csp_list_uu_t structures.
              The pool is aimed to save the overhead of reallocating memories for csp_list_uu_t structures.
@example      Refer to the example of SZ_POOL in general_util.h.
 */
SZ_POOL_INIT(ul, csp_list_uu_t, csp_list_uu_destroy, csp_list_uu_reset)
typedef sz_pool_t(ul) csp_pool_ul_t;
#define csp_pool_ul_init() sz_pool_init(ul)
#define csp_pool_ul_destroy(p) sz_pool_destroy(ul, p)
#define csp_pool_ul_get(p) sz_pool_get(ul, p)
#define csp_pool_ul_reset(p) sz_pool_reset(ul, p)

/*@abstract    The HashMap maps UMI-group-name (char*) to csp_list_uu_t (*).

@example (A simple example from khash.h)
KHASH_MAP_INIT_INT(32, char)
int main() {
    int ret, is_missing;
    khiter_t k;
    khash_t(32) *h = kh_init(32);
    k = kh_put(32, h, 5, &ret);
    kh_value(h, k) = 10;
    k = kh_get(32, h, 10);
    is_missing = (k == kh_end(h));
    k = kh_get(32, h, 5);
    kh_del(32, h, k);
    for (k = kh_begin(h); k != kh_end(h); ++k)
        if (kh_exist(h, k)) kh_value(h, k) = 1;
    kh_destroy(32, h);
    return 0;
}
 */
KHASH_MAP_INIT_STR(ug, csp_list_uu_t*)
typedef khash_t(ug) csp_map_ug_t;
#define csp_map_ug_iter khiter_t
#define csp_map_ug_init() kh_init(ug)
#define csp_map_ug_resize(h, s) kh_resize(ug, h, s)
#define csp_map_ug_put(h, k, r)  kh_put(ug, h, k, r)
#define csp_map_ug_get(h, k) kh_get(ug, h, k)
#define csp_map_ug_del(h, k) kh_del(ug, h, k)
#define csp_map_ug_exist(h, x) kh_exist(h, x)
#define csp_map_ug_key(h, x) kh_key(h, x)
#define csp_map_ug_val(h, x) kh_val(h, x)
#define csp_map_ug_begin(h) kh_begin(h)
#define csp_map_ug_end(h) kh_end(h)
#define csp_map_ug_size(h) kh_size(h)
#define csp_map_ug_reset(h) kh_clear(ug, h)
#define csp_map_ug_destroy(h) {								\
    if (h) {										\
        csp_map_ug_iter __k;								\
        for (__k = csp_map_ug_begin(h); __k != csp_map_ug_end(h); __k++) { 			\
            if (csp_map_ug_exist(h, __k)) csp_list_uu_destroy(csp_map_ug_val(h, __k));		\
        }											\
        kh_destroy(ug, h);									\
    }												\
}

/* Struct csp_list_qu_t APIs 
@abstract  The structure stores all qual value of one sample for certain query pos.
@param v   The csp_list_qu_t structure [csp_list_qu_t].

@example   Refer to the example in kvec.h.
 */
typedef kvec_t(int8_t) csp_list_qu_t;
#define csp_list_qu_init(v) kv_init(v)
#define csp_list_qu_resize(v, size) kv_resize(int8_t, v, size)
#define csp_list_qu_push(v, x) kv_push(int8_t, v, x)
#define csp_list_qu_A(v, i) kv_A(v, i)
#define csp_list_qu_size(v) kv_size(v)
#define csp_list_qu_max(v) kv_max(v)
#define csp_list_qu_destroy(v) kv_destroy(v)
#define csp_list_qu_reset(v) ((v).n = 0)

SZ_NUMERIC_OP_INIT(cu_d, double)
SZ_NUMERIC_OP_INIT(cu_s, size_t)

/*@abstract    Internal function to convert the base call quality score to related values for different genotypes.
@param qual    Qual value for the query pos in the read of the UMI gruop. The value is extracted by calling bam_get_qual() and 
               could be translated to qual char by plusing 33.
@param cap_bq   
@param min_bq
@param rv      Pointer of vector of loglikelihood for AA, AA+AB (doublet), AB, B or E (see Demuxlet paper online methods)
               Size of RV must be no less than 4. Be careful that no size check inside the function
@return        0 if success, -1 otherwise.

@reference     1. http://emea.support.illumina.com/bulletins/2016/04/fastq-files-explained.html
               2. https://linkinghub.elsevier.com/retrieve/pii/S0002-9297(12)00478-8 
 */
static inline int get_qual_vector(double qual, double cap_bq, double min_bq, double *rv) {
    double bq = max2(min2(cap_bq, qual), min_bq);
    double p = pow(0.1, bq / 10);
    rv[0] = log(1 - p);              rv[1] = log(0.75 - 2.0 / 3 * p);
    rv[2] = log(0.5 - 1.0 / 3 * p);  rv[3] = log(p);
    return 0;
}

/*@abstract     Internal function to translate qual matrix to vector of GL.
@param qm       Qual matrix: 5-by-4, columns are [1-Q, 3/4-2/3Q, 1/2-1/3Q, Q]. See csp_plp_t::qmat
@param bc       Base count: (5, ). See csp_plp_t::bc
@param ref_idx  Index of ref base in 'ACGTN'. 
@param alt_idx  Index of alt base in 'ACGTN'.
@param db       doublet_GL. 1/0.
@param gl       Array of GL: loglikelihood for 
                     GL1: L(rr|qual_matrix, base_count), 
                     GL2-GL5: L(ra|..), L(aa|..), L(rr+ra|..), L(ra+aa|..)
                Size must be no less than 5. Be careful that no size check inside the function
@param n        Pointer of num of elements in GL array.
@return         0 if success, -1 otherwise.

@note           TODO: In some special cases, ref=A and alt=AG for example, the ref_idx would be equal with alt_idx.
                Should be fixed in future.
 */
static int qual_matrix_to_geno(double qm[][4], size_t *bc, int8_t ref_idx, int8_t alt_idx, int db, double *gl, int *n) {
    int8_t other_idx[4], noth, i;
    size_t ref_read, alt_read;
    double *ref_qual, *alt_qual, oth_qual, tmp_qual;
    for (i = 0, noth = 0; i < 5; i++) {
        if (i != ref_idx && i != alt_idx) other_idx[noth++] = i;
    }
    ref_read = bc[ref_idx];
    alt_read = bc[alt_idx];
    ref_qual = qm[ref_idx];
    alt_qual = qm[alt_idx];
    for (i = 0, oth_qual = 0, tmp_qual = 0; i < noth; i++) {
        oth_qual += qm[other_idx[i]][3];
        tmp_qual += bc[other_idx[i]];
    }
    oth_qual += log(2.0 / 3) * tmp_qual;
    gl[0] = oth_qual + ref_qual[0] + alt_qual[3] + log(1.0 / 3) * alt_read;
    gl[1] = oth_qual + ref_qual[2] + alt_qual[2];
    gl[2] = oth_qual + ref_qual[3] + alt_qual[0] + log(1.0 / 3) * ref_read;
    if (db) {
        gl[3] = oth_qual + ref_qual[1] + log(1.0 / 4) * alt_read;
        gl[4] = oth_qual + alt_qual[1] + log(1.0 / 4) * ref_read;
        *n = 5;
    } else { *n = 3; }
    return 0;
}

/*@abstract     Infer ref and alt based on the read count of each possible alleles (ACGTN).
@param bc       Pointer of array that stores the read count of each possible alleles, the read count is
                in the order of A/C/G/T/N.
@param ref_idx  Pointer of index of ref in "ACGTN".
@param alt_idx  Pointer of index of alt in "ACGTN".
@return         Void.

@note           It's usually called when the input pos has no ref or alt.
 */
static inline void csp_infer_allele(size_t *bc, int8_t *ref_idx, int8_t *alt_idx) {
    int8_t i, k1, k2;
    size_t m1, m2;
    if (bc[0] < bc[1]) { m1 = bc[1]; m2 = bc[0]; k1 = 1; k2 = 0; }
    else { m1 = bc[0]; m2 = bc[1]; k1 = 0; k2 = 1; }
    for (i = 2; i < 5; i++) {
        if (bc[i] > m1) { m2 = m1; k2 = k1; m1 = bc[i]; k1 = i; }
        else if (bc[i] > m2) { m2 = bc[i]; k2 = i; }
    }
    *ref_idx = k1; *alt_idx = k2;
}

/*@abstract  The structure that store pileup info of one cell/sample for certain query pos.
@param bc    Total read count for each base in the order of 'ACGTN'.
@param tc    Total read count for all bases.
@param ad    Read count of alt.
@param dp    Read count of alt + ref.
@param oth   Read count of bases except alt and ref.
@param qu    All qual values of each base for one sample in the order of 'ACGTN'.
@param qmat  Matrix of qual with 'ACGTN' vs. [1-Q, 3/4-2/3Q, 1/2-1/3Q, Q].
@param gl    Array of GL: loglikelihood for 
               GL1: L(rr|qual_matrix, base_count), 
               GL2-GL5: L(ra|..), L(aa|..), L(rr+ra|..), L(ra+aa|..).
@param ngl   Num of valid elements in the array gl.
@param hug   Pointer of hash table that stores stat info of UMI groups.
 */
typedef struct {
    size_t bc[5];
    size_t tc, ad, dp, oth;
    csp_list_qu_t qu[5];
    double qmat[5][4];
    double gl[5];
    int ngl;
    csp_map_ug_t *hug;
} csp_plp_t;

/* note that the @p qu is also initialized after calling calloc(). */
static inline csp_plp_t* csp_plp_init(void) { return (csp_plp_t*) calloc(1, sizeof(csp_plp_t)); }

static inline void csp_plp_destroy(csp_plp_t *p) { 
    if (p) { 
        int i;
        for (i = 0; i < 5; i++) { csp_list_qu_destroy(p->qu[i]); }
        if (p->hug) { csp_map_ug_destroy(p->hug); }
        free(p); 
    }
}

static inline void csp_plp_reset(csp_plp_t *p) {
    if (p) {   // TODO: reset based on is_genotype.
        int i;
        memset(p->bc, 0, sizeof(p->bc));
        p->tc = p->ad = p->dp = p->oth = 0;
        for (i = 0; i < 5; i++) { csp_list_qu_reset(p->qu[i]); }
        memset(p->qmat, 0, sizeof(p->qmat));
        p->ngl = 0;
        if (p->hug) { csp_map_ug_reset(p->hug); }
    }
}

/*@abstract    Print the content to csp_plp_t to stream.
@param fp      Pointer of stream.
@param p       Pointer of csp_plpt_t to be printed.
@param prefix  Pointer of prefix string. Set to "" if no prefix.
@return        Void.
 */
static void csp_plp_print(FILE *fp, csp_plp_t *p, char *prefix) {
    int i, j;
    csp_map_ug_iter u;
    fprintf(fp, "%stotal read count = %ld\n", prefix, p->tc);
    fprintf(fp, "%sbase count (A/C/G/T/N):", prefix);
    for (i = 0; i < 5; i++) fprintf(fp, " %ld", p->bc[i]);
    fputc('\n', fp);
    fprintf(fp, "%squal matrix 5x4:\n", prefix);
    for (i = 0; i < 5; i++) {
        fprintf(fp, "%s\t", prefix);
        for (j = 0; j < 4; j++) fprintf(fp, " %.2f", p->qmat[i][j]);
        fputc('\n', fp);
    }
    fprintf(fp, "%snum of geno likelihood = %d\n", prefix, p->ngl);
    if (p->ngl) {
        fprintf(fp, "%sgeno likelihood:", prefix);
        for (i = 0; i < p->ngl; i++) fprintf(fp, " %.2f", p->gl[i]);
        fputc('\n', fp);
    }
    if (p->hug) {
        int size = csp_map_ug_size(p->hug);
        fprintf(fp, "%ssize of the csp_map_ug = %d\n", prefix, size);
        if (size) {
            fprintf(fp, "%s", prefix);
            for (u = csp_map_ug_begin(p->hug); u != csp_map_ug_end(p->hug); u++) {
                if (csp_map_ug_exist(p->hug, u)) { fprintf(fp, " %s", csp_map_ug_key(p->hug, u)); }
            } fputc('\n', fp);
        }
    }
}

/*@abstract     Format the content of csp_plp_t (the pileup stat info) of one cell/sample for certain query pos to 
                string in the output vcf file. 
@param p        Pointer of csp_plp_t structure corresponding to the pos.
@param s        Pointer of kstring_t which would store the formatted string.
@return         0 if success, -1 otherwise.
 */
static int csp_plp_str_vcf(csp_plp_t *p, kstring_t *s) {
    if (p->tc <= 0) { kputs(".:.:.:.:.:.", s); return 0; }
    double gl[5];
    int i, m;
    double tmp = -10 / log(10);
    char *gt[] = {"0/0", "1/0", "1/1"};
    m = get_idx_of_max(cu_d, p->gl, 3);
    kputs(gt[m], s);
    ksprintf(s, ":%ld:%ld:%ld:", p->ad, p->dp, p->oth);
    for (i = 0; i < p->ngl; i++) { gl[i] = p->gl[i] * tmp; }
    if (join_arr_to_str(cu_d, gl, p->ngl, ',', "%.0f", s) < p->ngl) { return -1; }
    kputc_(':', s);
    if (join_arr_to_str(cu_s, p->bc, 5, ',', "%ld", s) < 5) { return -1; }
    return 0;
}

static int csp_plp_to_vcf(csp_plp_t *p, jfile_t *s) {
    if (p->tc <= 0) { jf_puts(".:.:.:.:.:.", s); return 0; }
    double gl[5];
    int i, m;
    double tmp = -10 / log(10);
    char *gt[] = {"0/0", "1/0", "1/1"};
    m = get_idx_of_max(cu_d, p->gl, 3);
    jf_puts(gt[m], s);
    jf_printf(s, ":%ld:%ld:%ld:", p->ad, p->dp, p->oth);
    for (i = 0; i < p->ngl; i++) { gl[i] = p->gl[i] * tmp; }
    if (join_arr_to_str(cu_d, gl, p->ngl, ',', "%.0f", s->buf) < p->ngl) { return -1; }  // TODO: use internal buf directly is not good.
    jf_putc_(':', s);
    if (join_arr_to_str(cu_s, p->bc, 5, ',', "%ld", s->buf) < 5) { return -1; }
    return 0;
}

/*@abstract    The HashMap maps sample-group-name (char*) to csp_plp_t (*).
@example       Refer to a simple example in khash.h.
 */
KHASH_MAP_INIT_STR(sg, csp_plp_t*)
typedef khash_t(sg) csp_map_sg_t;
#define csp_map_sg_iter khiter_t
#define csp_map_sg_init() kh_init(sg)
#define csp_map_sg_clear(h) kh_clear(sg, h)
#define csp_map_sg_resize(h, s) kh_resize(sg, h, s)
#define csp_map_sg_put(h, k, r)  kh_put(sg, h, k, r)
#define csp_map_sg_get(h, k) kh_get(sg, h, k)
#define csp_map_sg_del(h, k) kh_del(sg, h, k)
#define csp_map_sg_exist(h, x) kh_exist(h, x)
#define csp_map_sg_key(h, x) kh_key(h, x)
#define csp_map_sg_val(h, x) kh_val(h, x)
#define csp_map_sg_begin(h) kh_begin(h)
#define csp_map_sg_end(h) kh_end(h)
#define csp_map_sg_size(h) kh_size(h)
#define csp_map_sg_destroy(h) {								\
    if (h) {											\
        csp_map_sg_iter __k;									\
        for (__k = csp_map_sg_begin(h); __k != csp_map_sg_end(h); __k++) { 			\
            if (csp_map_sg_exist(h, __k)) csp_plp_destroy(csp_map_sg_val(h, __k)); 	\
        }										\
        kh_destroy(sg, h);									\
    }												\
}
#define csp_map_sg_reset_val(h) {								\
    if (h) {											\
        csp_map_sg_iter __k;									\
        for (__k = csp_map_sg_begin(h); __k != csp_map_sg_end(h); __k++) {			\
            if (csp_map_sg_exist(h, __k)) csp_plp_reset(csp_map_sg_val(h, __k)); 		\
        }											\
    }												\
}

/*@abstract  The structure stores the stat info of all sample groups for certain query pos.
@param ref_idx  Index of ref in "ACGTN". Negative number means not valid value.
@param alt_idx  Index of alt in "ACGTN". Negative number means not valid value.
@param inf_rid  Infered index of ref in "ACGTN". Negative number means not valid value.
@param inf_aid  Infered index of alt in "ACGTN". Negative number means not valid value.
@param bc    Read count of each base summarizing all sample groups for the pos, in the order of 'ACGTN'.
@param tc    Total read count of all bases for the pos.
@param ad    Read count of alt.
@param dp    Read count of alt + ref.
@param oth   Read count of bases except alt and ref.
@param nr_*  Num of records/lines outputed to mtx file for certain SNP/pos.
@param hsg   HashMap that stores the stat info of all sample groups for the pos.
@param hsg_iter Pointer of array of csp_map_sg_iter. The iter in the array is in the same order of sg names.
@param nsg   Size of csp_map_sg_iter array hsg_iter.
@param pu    Pool of csp_umi_unit_t structures.
@param pl    Pool of csp_list_uu_t structures.
@param su    Pool of UMI strings.
@param qvec  A container for the qual vector returned by get_qual_vector().
 */
typedef struct {
    int8_t ref_idx, alt_idx, inf_rid, inf_aid;
    size_t bc[5];
    size_t tc, ad, dp, oth;
    size_t nr_ad, nr_dp, nr_oth;
    csp_map_sg_t *hsg;
    csp_map_sg_iter *hsg_iter;
    int nsg;
    csp_pool_uu_t *pu;
    csp_pool_ul_t *pl;
    csp_pool_ps_t *su;
    double qvec[4];
} csp_mplp_t;

/*@abstract  Initialize the csp_mplp_t structure.
@return      Pointer to the csp_mplp_t structure if success, NULL otherwise.

@note        1. The kstring_t s is also initialized inside this function.   
             2. The valid pointer returned by this function should be freed by csp_mplp_destroy() function
                   when no longer used.
 */
static inline csp_mplp_t* csp_mplp_init(void) { 
    csp_mplp_t *p = (csp_mplp_t*) calloc(1, sizeof(csp_mplp_t));
    return p;
}

static inline void csp_mplp_destroy(csp_mplp_t *p) { 
    if (p) {
        if (p->hsg) { csp_map_sg_destroy(p->hsg); }
        if (p->hsg_iter) { free(p->hsg_iter); }
        if (p->pu) { csp_pool_uu_destroy(p->pu); }
        if (p->pl) { csp_pool_ul_destroy(p->pl); }
        if (p->su) { csp_pool_ps_destroy(p->su); }
        free(p); 
    }
}

static inline void csp_mplp_reset(csp_mplp_t *p) {
    if (p) {
        memset(p->bc, 0, sizeof(p->bc));
        p->tc = p->ad = p->dp = p->oth = 0;
        p->nr_ad = p->nr_dp = p->nr_oth = 0;
        if (p->hsg) { csp_map_sg_reset_val(p->hsg); }
        if (p->pu) { csp_pool_uu_reset(p->pu); }
        if (p->pl) { csp_pool_ul_reset(p->pl); }
        if (p->su) { csp_pool_ps_reset(p->su); }
        memset(p->qvec, 0, sizeof(p->qvec));
    }
}

/*@abstract    Print the content to csp_mplp_t to stream.
@param fp      Pointer of stream.
@param p       Pointer of csp_mplpt_t to be printed.
@param prefix  Pointer of prefix string. Set to "" if no prefix.
@return        Void.
 */
static void csp_mplp_print(FILE *fp, csp_mplp_t *p, char *prefix) {
    int i;
    csp_plp_t *plp;
    kstring_t ks = KS_INITIALIZE, *s = &ks;
    fprintf(fp, "%sref_idx = %d, alt_idx = %d\n", prefix, p->ref_idx, p->alt_idx);
    fprintf(fp, "%sinf_rid = %d, inf_aid = %d\n", prefix, p->inf_rid, p->inf_aid);
    fprintf(fp, "%stotal base count = %ld\n", prefix, p->tc);
    fprintf(fp, "%sbase count (A/C/G/T/N):", prefix);
    for (i = 0; i < 5; i++) { fprintf(fp, " %ld", p->bc[i]); }
    fputc('\n', fp);
    fprintf(fp, "%snum of sample group = %d\n", prefix, p->nsg);
    if (p->nsg) {
        kputs(prefix, s); kputc('\t', s);
        for (i = 0; i < p->nsg; i++) {
            fprintf(fp, "%sSG-%d = %s:\n", prefix, i, csp_map_sg_key(p->hsg, p->hsg_iter[i]));
            plp = csp_map_sg_val(p->hsg, p->hsg_iter[i]);
            csp_plp_print(fp, plp, ks_str(s));
        }
    }
    ks_free(s);
}

static inline void csp_mplp_print_(FILE *fp, csp_mplp_t *p, char *prefix) {
    int i;
    fprintf(fp, "%sref_idx = %d, alt_idx = %d\n", prefix, p->ref_idx, p->alt_idx);
    fprintf(fp, "%sinf_rid = %d, inf_aid = %d\n", prefix, p->inf_rid, p->inf_aid);
    fprintf(fp, "%stotal base count = %ld\n", prefix, p->tc);
    fprintf(fp, "%sbase count (A/C/G/T/N):", prefix);
    for (i = 0; i < 5; i++) { fprintf(fp, " %ld", p->bc[i]); }
    fputc('\n', fp);
    fprintf(fp, "%snum of sample group = %d\n", prefix, p->nsg);
}

/*@abstract  Set sample group names for the csp_mplp_t structure.
@param p     Pointer to the csp_mplp_t structure.
@parma s     Pointer to the array of names of sample groups.
@param n     Num of sample groups.
@return      0, no error; -1 otherwise.

@note        1. This function should be called just one time right after csp_mplp_t structure was created
                becuase the sgname wouldn't change once set.
             2. The HashMap (for sgnames) in csp_mplp_t should be empty or NULL.
             3. The keys of HashMap are exactly pointers to sg names coming directly from @p s.
 */
static int csp_mplp_set_sg(csp_mplp_t *p, char **s, const int n) {
    if (NULL == p || NULL == s || 0 == n) { return -1; }
    int i, r;
    csp_map_sg_iter k;
    if (NULL == p->hsg && NULL == (p->hsg = csp_map_sg_init())) { return -1; }
    if (NULL == p->hsg_iter && NULL == (p->hsg_iter = (csp_map_sg_iter*) malloc(sizeof(csp_map_sg_iter) * n))) { return -1; }
    for (i = 0; i < n; i++) {
        if (s[i]) { 
            k = csp_map_sg_put(p->hsg, s[i], &r); 
            if (r <= 0) { csp_mplp_destroy(p); return -1; } /* r = 0 means repeatd sgnames. */
            else { csp_map_sg_val(p->hsg, k) = NULL; }
        } else { return -1; }
    }
    /* Storing iter index for each sg (sample group) name must be done after all sg names have been pushed into 
       the HashMap in case that the internal arrays of HashMap autoly shrink or some else modifications. */
    for (i = 0; i < n; i++) {
        k = csp_map_sg_get(p->hsg, s[i]);
        if (k == csp_map_sg_end(p->hsg)) { return -1; }
        else { p->hsg_iter[i] = k; }
    }
    p->nsg = n;
    return 0;
}

/*@abstract  Format the content of csp_mplp_t (the pileup stat info) of certain query pos to string in the output vcf file.
@param mplp  Pointer of the csp_mplp_t structure corresponding to the pos.
@param s     Pointer of kstring_t which stores the formatted string.
@return      0 if success, -1 otherwise.
 */
static inline int csp_mplp_str_vcf(csp_mplp_t *mplp, kstring_t *s) {
    int i;
    for (i = 0; i < mplp->nsg; i++) {
        kputc_('\t', s);
        if (csp_plp_str_vcf(csp_map_sg_val(mplp->hsg, mplp->hsg_iter[i]), s) < 0) { return -1; }
    } //s->s[--(s->l)] = '\0';    /* s->l could not be negative unless no csp_plp_t(s) are printed to s->s. */
    return 0;
}

static inline int csp_mplp_to_vcf(csp_mplp_t *mplp, jfile_t *s) {
    int i;
    for (i = 0; i < mplp->nsg; i++) {
        jf_putc_('\t', s);
        if (csp_plp_to_vcf(csp_map_sg_val(mplp->hsg, mplp->hsg_iter[i]), s) < 0) { return -1; }
    } //s->s[--(s->l)] = '\0';    /* s->l could not be negative unless no csp_plp_t(s) are printed to s->s. */
    return 0;
}

/*@abstract    Format the content of csp_mplp_t of certain query pos to string in the output sparse matrices file.
@param mplp    Pointer of the csp_mplp_t structure corresponding to the pos.
@param ks_ad   Pointer of kstring_t which is to store formatted AD string.
@param ks_dp   Pointer of kstring_t which is to store formatted DP string.
@param ks_oth  Pointer of kstring_t which is to store formatted OTH string.
@param idx     Index of the SNP/mplp (1-based).
@return        0 if success, -1 otherwise.
 */
static inline int csp_mplp_str_mtx(csp_mplp_t *mplp, kstring_t *ks_ad, kstring_t *ks_dp, kstring_t *ks_oth, size_t idx) {
    csp_plp_t *plp;
    int i;
    for (i = 1; i <= mplp->nsg; i++) {
        plp = csp_map_sg_val(mplp->hsg, mplp->hsg_iter[i]);
        if (plp->ad) ksprintf(ks_ad, "%ld\t%d\t%ld\n", idx, i, plp->ad);
        if (plp->dp) ksprintf(ks_dp, "%ld\t%d\t%ld\n", idx, i, plp->dp);
        if (plp->oth) ksprintf(ks_oth, "%ld\t%d\t%ld\n", idx, i, plp->oth);        
    }
    return 0; 
}

/*@abstract    Format the content of csp_mplp_t of certain query pos to string in the tmp output sparse matrices file.
@param mplp    Pointer of the csp_mplp_t structure corresponding to the pos.
@param ks_ad   Pointer of kstring_t which is to store formatted AD string.
@param ks_dp   Pointer of kstring_t which is to store formatted DP string.
@param ks_oth  Pointer of kstring_t which is to store formatted OTH string.
@return        0 if success, -1 otherwise.

@note          This function is used for tmp files.
 */
static inline int csp_mplp_str_mtx_tmp(csp_mplp_t *mplp, kstring_t *ks_ad, kstring_t *ks_dp, kstring_t *ks_oth) {
    csp_plp_t *plp;
    int i;
    for (i = 1; i <= mplp->nsg; i++) {
        plp = csp_map_sg_val(mplp->hsg, mplp->hsg_iter[i]);
        if (plp->ad) ksprintf(ks_ad, "%d\t%ld\n", i, plp->ad);
        if (plp->dp) ksprintf(ks_dp, "%d\t%ld\n", i, plp->dp);
        if (plp->oth) ksprintf(ks_oth, "%d\t%ld\n", i, plp->oth);        
    }
    kputc('\n', ks_ad); kputc('\n', ks_dp); kputc('\n', ks_oth);
    return 0; 
}

static int csp_mplp_to_mtx(csp_mplp_t *mplp, jfile_t *fs_ad, jfile_t *fs_dp, jfile_t *fs_oth, size_t idx) {
    csp_plp_t *plp;
    int i;
    for (i = 1; i <= mplp->nsg; i++) {
        plp = csp_map_sg_val(mplp->hsg, mplp->hsg_iter[i - 1]);
        if (plp->ad) fs_ad->is_tmp ? jf_printf(fs_ad, "%d\t%ld\n", i, plp->ad) : jf_printf(fs_ad, "%ld\t%d\t%ld\n", idx, i, plp->ad);
        if (plp->dp) fs_dp->is_tmp ? jf_printf(fs_dp, "%d\t%ld\n", i, plp->dp) : jf_printf(fs_dp, "%ld\t%d\t%ld\n", idx, i, plp->dp);
        if (plp->oth) fs_oth->is_tmp ? jf_printf(fs_oth, "%d\t%ld\n", i, plp->oth) : jf_printf(fs_oth, "%ld\t%d\t%ld\n", idx, i, plp->oth);      
    }
    if (fs_ad->is_tmp) jf_putc('\n', fs_ad);
    if (fs_dp->is_tmp) jf_putc('\n', fs_dp);
    if (fs_oth->is_tmp) jf_putc('\n', fs_oth);
    return 0; 
}

#if DEVELOP
/* 
* Tags for sparse matrices
* It's useful when the input tags are optional.
*/
typedef size_t csp_mtx_value_t;
typedef int csp_mtx_iter_t;
typedef csp_mtx_value_t (*csp_mtx_value_func_t)(csp_mplp_t*, csp_plp_t*);

static inline csp_mtx_value_t csp_mtx_value_AD(csp_mplp_t *mplp, csp_plp_t *plp) {
    return plp->ad;
}

static inline csp_mtx_value_t csp_mtx_value_DP(csp_mplp_t *mplp, csp_plp_t *plp) {
    return plp->dp;
}

static inline csp_mtx_value_t csp_mtx_value_OTH(csp_mplp_t *mplp, csp_plp_t *plp) {
    return plp->oth;
}

static csp_mtx_iter_t csp_mtx_ntags = 3;
static char *csp_mtx_tags[] = {"AD", "DP", "OTH"};
static csp_mtx_value_func_t csp_mtx_value_funcs[] = { &csp_mtx_value_AD, &csp_mtx_value_DP, &csp_mtx_value_OTH };

static inline char* csp_get_mtx_fn(char *tag) {
    kstring_t ks = KS_INITIALIZE;
    ksprintf(&ks, "cellSNP.tag.%s.mtx", tag);
    char *p = strdup(ks_str(&ks));
    ks_free(&ks);
    return p;
}

static inline csp_mtx_iter_t csp_get_mtx_idx(char *tag) {
    csp_mtx_iter_t i;
    for (i = 0; i < csp_mtx_ntags; i++) {
        if (0 == strcmp(tag, csp_mtx_tags[i])) { return i; }
    } 
    return -1;
}

static inline csp_mtx_value_func_t csp_get_mtx_value_func(csp_mtx_iter_t i) { return csp_mtx_value_funcs[i]; }

typedef struct {
    char *out_fn;
    csp_mtx_value_func_t vfunc;
} csp_mtx_tag_fs;

static inline csp_mtx_tag_fs* csp_mtx_tag_fs_init(void) {
    return (csp_mtx_tag_fs*) calloc(1, sizeof(csp_mtx_tag_fs));
}

static inline void sp_mtx_tag_fs_destroy(csp_mtx_tag_fs *p) {
    if (p) { free(p->out_fn); free(p); }
}
#endif

#endif
