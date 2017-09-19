#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <time.h>

#include "platform-udp.h"
#include "zhe.h"
#include "zhe-tracing.h"
#include "zhe-assert.h"

#include "zhe-config-deriv.h" /* for N_OUT_CONDUITS, ZTIME_TO_SECu32 */

#include "testlib.h"

zhe_paysize_t getrandomid(unsigned char *ownid, size_t ownidsize)
{
    FILE *fp;
    if ((fp = fopen("/dev/urandom", "rb")) == NULL) {
        perror("can't open /dev/urandom\n");
        exit(1);
    }
    if (fread(ownid, ownidsize, 1, fp) != 1) {
        fprintf(stderr, "can't read %zu random bytes from /dev/urandom\n", ownidsize);
        fclose(fp);
        exit(1);
    }
    fclose(fp);
    return (zhe_paysize_t)ownidsize;
}

zhe_paysize_t getidfromarg(unsigned char *ownid, size_t ownidsize, const char *in)
{
    size_t i = 0;
    int pos = 0, dpos;
    while(i < ownidsize && in[pos] && sscanf(in + pos, "%hhx%n", &ownid[i], &dpos) == 1) {
        pos += dpos;
        if (in[pos] == ':') {
            pos++;
        } else if (in[pos] != 0) {
            fprintf(stderr, "junk in explicit peer id\n");
            exit(1);
        }
        i++;
    }
    if (in[pos]) {
        fprintf(stderr, "junk at end of explicit peer id\n");
        exit(1);
    }
    return (zhe_paysize_t)i;
}

void cfg_handle_addrs(struct zhe_config *cfg, struct zhe_platform *platform, const char *scoutaddrstr, const char *mcgroups_join_str, const char *mconduit_dstaddrs_str)
{
    static struct zhe_address scoutaddr; /* FIXME: 't is a bit ugly this way */
    char *str;
    cfg->scoutaddr = &scoutaddr;
    if (!zhe_platform_string2addr(platform, cfg->scoutaddr, scoutaddrstr)) {
        fprintf(stderr, "%s: invalid address\n", scoutaddrstr); exit(2);
    } else {
        zhe_platform_join(platform, cfg->scoutaddr);
    }

    static struct zhe_address mcgroups_join[MAX_MULTICAST_GROUPS]; /* FIXME: 't is a bit ugly this way */
    cfg->n_mcgroups_join = 0;
    cfg->mcgroups_join = mcgroups_join;
    str = strdup(mcgroups_join_str);
    for (char *addrstr = strtok(str, ","); addrstr != NULL; addrstr = strtok(NULL, ",")) {
        if (cfg->n_mcgroups_join == MAX_MULTICAST_GROUPS) {
            fprintf(stderr, "too many multicast groups specified\n"); exit(2);
        } else if (!zhe_platform_string2addr(platform, &cfg->mcgroups_join[cfg->n_mcgroups_join], addrstr)) {
            fprintf(stderr, "%s: invalid address\n", addrstr); exit(2);
        } else if (!zhe_platform_join(platform, &cfg->mcgroups_join[cfg->n_mcgroups_join])) {
            fprintf(stderr, "%s: join failed\n", addrstr); exit(2);
        } else {
            cfg->n_mcgroups_join++;
        }
    }
    free(str);

    static struct zhe_address mconduit_dstaddrs[N_OUT_MCONDUITS]; /* FIXME: 't is a bit ugly this way */
    cfg->n_mconduit_dstaddrs = 0;
    cfg->mconduit_dstaddrs = mconduit_dstaddrs;
    str = strdup(mconduit_dstaddrs_str);
    for (char *addrstr = strtok(str, ","); addrstr != NULL; addrstr = strtok(NULL, ",")) {
        if (cfg->n_mconduit_dstaddrs == N_OUT_MCONDUITS) {
            fprintf(stderr, "too many mconduit dstaddrs specified\n"); exit(2);
        } else if (!zhe_platform_string2addr(platform, &cfg->mconduit_dstaddrs[cfg->n_mconduit_dstaddrs], addrstr)) {
            fprintf(stderr, "%s: invalid address\n", addrstr); exit(2);
        } else {
            cfg->n_mconduit_dstaddrs++;
        }
    }
    if (cfg->n_mconduit_dstaddrs != N_OUT_MCONDUITS) {
        fprintf(stderr, "too few mconduit dstaddrs specified\n"); exit(2);
    }
    free(str);
}
