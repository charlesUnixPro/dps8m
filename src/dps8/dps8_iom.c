/**
 * \file dps8_iom.c
 * \project dps8
 * \date 9/21/12
 *  Adapted by Harry Reed on 9/21/12.
 */

#include <stdio.h>

#include "dps8.h"


// console stuff

/* bootload_console$poll_for_console (used when there is no config deck)
 * tries channels 8-63 (10o-77o) on the tape IOM.
 * iom_setup (for now) sets this to 8.  It should be specified in
 * iom.conf
 */

int console_chan = 0;


// iom stuff ...

word18 iom_pos;

/* These are from iom_word_macros */

void decode_pcw(word36 pcw1, word36 pcw2) {
    word8 command, device, extension, chan_ctrl, chan_cmd, chan_data;
    word8 channel;
    int mask;
    printf("pcw ");
    /*
     if(((pcw1 & 0700000) != 0700000) ||
     (pcw2 & 0700000000000LL) || (pcw2 & 0000777777777LL))
     printf("(not actually a pcw) ");
     */
    command = (pcw1 & 0770000000000LL) >> 30;
    device = (pcw1 & 0007700000000LL) >> 24;
    extension = (pcw1 & 0000077000000LL) >> 18;
    mask = (pcw1 & 0040000) >> 14;
    chan_ctrl = (pcw1 & 0030000) >> 12;
    chan_cmd = (pcw1 & 0007700) >> 6;
    chan_data = pcw1 & 0000077;
    channel = (pcw2 & 0077000000000LL) >> 27;
    printf("cmd:%d ch:%d dev:%d %s %s chdat:%d ext:%d%s\n",
           command, channel, device,
           !chan_cmd ? "rec" :
           (chan_cmd == 2 ? "nondat" :
            (chan_cmd == 6 ? "multi" :
             (chan_cmd == 8 ? "char" : "?"))),
           !chan_ctrl ? "term" :
           (chan_ctrl == 2 ? "proceed" :
            (chan_ctrl == 3 ? "marker" : "?")),
           chan_data, extension, mask ? " +mask" : "");
}

void decode_ddcw(word36 ddcw) {
    word8 char_offset, op_type;
    int tally_type;
    word36 address;
    word18 tally;
    address = (ddcw & 0777777000000LL) >> 18;
    char_offset = (ddcw & 0000000700000LL) >> 15;
    tally_type = (ddcw & 0000000040000LL) >> 14;
    op_type = (ddcw & 0000000030000LL) >> 12;
    tally = ddcw & 0000000007777LL;
    printf("ddcw addr:%6.6llo tally:%4.4lo(%s) op:%s choff:%d\n",
           address, tally, tally_type ? "ch" : "word",
           !op_type ? "iotd" :
           (op_type == 1 ? "iotp" :
            (op_type == 3 ? "iontp" : "?")), char_offset);
}

void decode_idcw(word36 idcw) {
    word8 command, device, extension, chan_ctrl, chan_cmd, chan_data;
    int mask;
    printf("idcw ");
    /*
     if((idcw & 0700000) != 0700000)
     printf("(not actually an idcw) ");
     */
    command = (idcw & 0770000000000LL) >> 30;
    device = (idcw & 0007700000000LL) >> 24;
    extension = (idcw & 0000077000000LL) >> 18;
    mask = (idcw & 0040000) >> 14;
    chan_ctrl = (idcw & 0030000) >> 12;
    chan_cmd = (idcw & 0007700) >> 6;
    chan_data = idcw & 0000077;
    printf("cmd:%d dev:%d %s %s chdat:%d ext:%d%s\n",
           command, device,
           !chan_cmd ? "rec" :
           (chan_cmd == 2 ? "nondat" :
            (chan_cmd == 6 ? "multi" :
             (chan_cmd == 8 ? "char" : "?"))),
           !chan_ctrl ? "term" :
           (chan_ctrl == 2 ? "proceed" :
            (chan_ctrl == 3 ? "marker" : "?")),
           chan_data, extension, mask ? " +mask" : "");
}

void decode_tdcw(word36 tdcw) {
    word8 bits;
    word36 address;
    printf("tdcw ");
    /*
     if((tdcw & 0777770) != 0030000)
     printf("(not actually a tdcw) ");
     */
    address = (tdcw & 0777777000000LL) >> 18;
    bits = tdcw & 0000007;
    printf("addr:%6.6llo bits:%d\n", address, bits);
}

/* These are from AN87, sec 3 */

void decode_lpw(word36 lpw1, word36 lpw2) {
    word36 dcw_addr, idcwp;
    word18 bound, base, tally;
    int res, iom_rel, se, nc, tal, rel;
    dcw_addr = (lpw1 & 0777777000000LL) >> 18;
    res = (lpw1 & 0400000) >> 17;
    iom_rel = (lpw1 & 0400000) >> 16;
    se = (lpw1 & 0400000) >> 15;
    nc = (lpw1 & 0400000) >> 14;
    tal = (lpw1 & 0400000) >> 13; /* tal and rel might be swapped */
    rel = (lpw1 & 0400000) >> 12;
    tally = lpw1 & 0007777;
    base = (lpw2 & 0777000000000LL) >> 27;
    bound = (lpw2 & 0000777000000LL) >> 18;
    idcwp = lpw2 & 0777777;
    printf(
           "lpw dcw:%6.6llo%s%s%s%s%s%s tally:%4.4lo %3.3lo %3.3lo %6.6llo\n",
           dcw_addr, res ? " res" : "", iom_rel ? " iom_rel" : "",
           se ? " se" : "", nc ? " nc" : "", tal ? " tal" : "", rel ? " rel" : "",
           tally, base, bound, idcwp);
}

void decode_scw(word36 scw) {
    word18 address, tally;
    int lq;
    printf("scw ");
    if(scw & 0170000)
        printf("(not an scw) ");
    address = (scw & 0777777000000LL) >> 18;
    tally = scw & 0007777;
    lq = (scw & 0600000) >> 16;
    printf("addr:%9.9lo lq:%d tally:%6.6lo\n", address, lq, tally);
}

/* Actual operations */
/* XXX: these OUGHT to return fault codes or interrupt numbers! */

/* XXX: this is static for now */
FILE *chan40;

word36 rec[2048]; /* XXX - max record size? */
word36 reclen; /* must match maketape.c */
word36 is_eof;

int iom_setup(const char *confname) {
    FILE *conf;
    conf = fopen(confname, "r");
    if(!conf) {
        fprintf(stderr, "could not open %s for reading\n", confname);
        return(1);
    }
    /* XXX: parse conf file */
    fclose(conf);
    /* XXX: this is static for now */
    chan40 = fopen("boot.tape", "r+b");
    if(!chan40) {
        fputs("could not open boot.tape\n", stderr);
        return(1);
    }
    console_chan = 010;
    return 0;
}

void iom_dcw(word18 dcwaddr) {
    word36 dcw;
    core_read(dcwaddr, &dcw);
    if((dcw & 0700000) == 0700000) {
        decode_idcw(dcw);
        /* read record? */
        if(((dcw & 0770000000000LL) >> 30) == 5) {
            /* this is sort of hairy */
            fread(&reclen, sizeof(reclen), 1, chan40);
            fread(&is_eof, sizeof(is_eof), 1, chan40);
            reclen -= 2 * sizeof(word36);
            fread(&rec, reclen, 1, chan40);
            reclen /= sizeof(word36); /* bytes to words */
            printf("Read record of %lld words\n", reclen);
        }
    } else {
        if((dcw & 0030000) == 0020000) {
        } else {
            word18 addr;
            decode_ddcw(dcw);
            decode_tdcw(dcw);
            addr = GETHI(dcw);
            /* IOTD? */
            if(!(dcw & 0030000)) {
                word18 pos;
                word36 xed1, xed2;
                for(pos = 0 ; pos < reclen ; ++pos) {
                    core_write(addr + pos, &rec[pos]);
                }
                printf("Transmitted record of %lld words to %6.6lo\n",
                       reclen, addr);
                /* disconnect? interrupt ??? */
                core_read2(30 /* decimal */, &xed1, &xed2);
// XXX                execdouble(xed1, xed2);
            }
        }
    }
}

void iom_pcw(word18 pcwaddr) {
    word36 pcw1, pcw2;
    word8 channel;
    word18 mboxaddr;
    word36 lpw, lpwext, scw;
    word18 tally, which_dcw;
    word18 dcwaddr;
    core_read2(pcwaddr, &pcw1, &pcw2);
    decode_pcw(pcw1, pcw2);
    /* XXX */
    channel = (pcw2 & 0077000000000LL) >> 27;
    mboxaddr = iom_pos + 4 * channel;
    core_read2(mboxaddr, &lpw, &lpwext);
    decode_lpw(lpw, lpwext);
    tally = lpw & 0007777;
    core_read(mboxaddr + 2, &scw);
    decode_scw(scw);
    /* ?? dcw = core[mboxaddr + 3]; */
    dcwaddr = (lpw & 0777777000000LL) >> 18;
    for(which_dcw = 0 ; which_dcw < tally ; ++which_dcw) {
        iom_dcw(dcwaddr + which_dcw);
    }
}

void iom_cioc(word8 conchan) {
    word18 lpwaddr, lpwtarget, tally;
    word36 lpw, lpwext;
    lpwaddr = iom_pos + 4 * conchan;
    core_read2(lpwaddr, &lpw, &lpwext);
    decode_lpw(lpw, lpwext);
    lpwtarget = (lpw & 0777777000000LL) >> 18;
    tally = lpw & 0007777;
    /* Special behavior for connect channel 2 */
    if(conchan == 2)
        iom_pcw(lpwtarget);
    else
        iom_dcw(lpwtarget);
}

void iom_cioc_cow(word18 cowaddr) {
    word36 cow;
    word8 chanptr, memport;
    core_read(cowaddr, &cow);
    chanptr = (cow & 0000070) >> 3;
    memport = cow & 0000007;
    printf("Connect COW: chanptr=%o memport=%o\n", chanptr, memport);
    /* XXX: what next? */
}

