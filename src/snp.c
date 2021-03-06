/* SNP operatoins API/routine
 * Author: Xianjie Huang <hxj5@hku.hk>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "htslib/vcf.h"
#include "htslib/sam.h"
#include "kvec.h"
#include "jstring.h"
#include "snp.h"

/* 
* SNP List API
*/

/*@note      The pointer returned successfully by csp_snp_init() should be freed
             by csp_snp_destroy() when no longer used.
 */
inline csp_snp_t* csp_snp_init(void) { return (csp_snp_t*) calloc(1, sizeof(csp_snp_t)); }

inline void csp_snp_destroy(csp_snp_t *p) { 
    if (p) { free(p->chr); free(p); } 
}

inline void csp_snp_reset(csp_snp_t *p) {
    if (p) { free(p->chr); memset(p, 0, sizeof(csp_snp_t)); }
}

/*@note        If length of Ref or Alt is larger than 1, then the SNP would be skipped.
               If length of Ref or Alt is 0, then their values would be infered during pileup.
 */
size_t get_snplist_from_vcf(const char *fn, csp_snplist_t *pl, int *ret, int print_skip) {
    htsFile *fp = NULL;
    bcf_hdr_t *hdr = NULL;
    bcf1_t *rec = NULL;
    csp_snp_t *ip = NULL;
    size_t l, m, n = 0;  /* use n instead of kv_size(*pl) in case that pl is not empty at the beginning. */
    int r;
    *ret = -1;
    if (NULL == fn || NULL == pl) { return 0; }
    if (NULL == (fp = hts_open(fn, "rb"))) { fprintf(stderr, "[E::%s] could not open '%s'\n", __func__, fn); return 0; }
    if (NULL == (hdr = bcf_hdr_read(fp))) { fprintf(stderr, "[E::%s] could not read header for '%s'\n", __func__, fn); goto fail; }
    if (NULL == (rec = bcf_init1())) { fprintf(stderr, "[E::%s] could not initialize the bcf structure.\n", __func__); goto fail; }
    for (m = 1; (r = bcf_read1(fp, hdr, rec)) >= 0; m++) {
        if (NULL == (ip = csp_snp_init())) { 
            fprintf(stderr, "[E::%s] could not initialize the csp_snp_t structure.\n", __func__); 
            goto fail; 
        }
        ip->chr = safe_strdup(bcf_hdr_id2name(hdr, rec->rid));
        if (NULL == ip->chr) {
            if (print_skip) { fprintf(stderr, "[W::%s] skip No.%ld SNP: could not get chr name.\n", __func__, m); }
            csp_snp_destroy(ip);
            continue;
        } ip->pos = rec->pos;
        bcf_unpack(rec, BCF_UN_STR);
        if (rec->n_allele > 0) {
            if (1 == (l = strlen(rec->d.allele[0]))) { ip->ref = rec->d.allele[0][0]; }
            else if (l > 1) { 
                if (print_skip) { fprintf(stderr, "[W::%s] skip No.%ld SNP: ref_len > 1.\n", __func__, m); }
                csp_snp_destroy(ip); 
                continue; 
            } // else: do nothing. keep ref = 0, 0 is its init value.               
            if (2 == rec->n_allele) {
                if (1 == (l = strlen(rec->d.allele[1]))) { ip->alt = rec->d.allele[1][0]; }
                else if (l > 1) {
                    if (print_skip) { fprintf(stderr, "[W::%s] skip No.%ld SNP: alt_len > 1.\n", __func__, m); }
                    csp_snp_destroy(ip);
                    continue; 					
                } // else: do nothing. keep alt = 0, 0 is its init value.
            } else if (rec->n_allele > 2) {
                if (print_skip) { fprintf(stderr, "[W::%s] skip No.%ld SNP: n_allele > 2.\n", __func__, m); }
                csp_snp_destroy(ip);
                continue;                 
            } // else: keep alt = 0.
        } // else: do nothing. keep ref = alt = 0, 0 is their init values.
        csp_snplist_push(*pl, ip);
        n++;
    }
    if (-1 == r) { // end of bcf file.
        csp_snplist_resize(*pl, csp_snplist_size(*pl));
    } else { 
        fprintf(stderr, "[E::%s] error when parsing '%s'\n", __func__, fn); 
        goto fail; 
    }
    bcf_destroy1(rec);
    bcf_hdr_destroy(hdr);
    hts_close(fp);
    *ret = 0;
    return n;
  fail:
    if (rec) bcf_destroy1(rec);
    if (hdr) bcf_hdr_destroy(hdr);
    if (fp) hts_close(fp);
    return n;
}

