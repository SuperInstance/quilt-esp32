/* qm_tables.h — generated-table struct typedefs shared by host harness and
 * firmware, plus the compiled program tables. Shapes follow qm_bench.c
 * (SuperInstance/cell-cascade, branch edge-benchmarks) exactly; qm2c.py
 * output assumes these typedefs pre-exist. */
#ifndef QM_TABLES_H
#define QM_TABLES_H

/* ── generated-table shapes ── */
typedef struct { const char *key, *canon; } QmKv;
typedef struct { const char *target, *canon; } QmBind;
typedef struct { const char *from, *to, *type; } QmLink;
enum { QM_SET, QM_EXPR };
typedef struct {
    const char *target;
    const char *kind;      /* NULL = any kind */
    int n_pe;
    const QmKv *pe;        /* payload_equals entries (canon values) */
    int action;            /* QM_SET | QM_EXPR */
    const char *set_canon; /* QM_SET */
    const char *centroid;  /* QM_EXPR: bound cell names */
    const char *sigma;
} QmRule;
typedef struct { const char *name, *target; } QmViewDef;
typedef struct {
    const char *to, *kind;
    int n_payload;
    const QmKv *payload;   /* canon values */
} QmSignal;

/* the compiled program tables (see qm2c.py) */
#ifdef QM_PROG_HEADER
#include QM_PROG_HEADER
#else
#include "qm_prog.h"
#endif

#endif /* QM_TABLES_H */
