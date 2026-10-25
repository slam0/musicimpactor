#include "mi_lib.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAXC 96
#define MAXT 160

/* ------------------------------------------------------------------ raport */

typedef struct { char *buf; int cap; int len; } rep_t;

static void rep(rep_t *r, const char *fmt, ...)
{
    va_list ap;
    int n;
    if (!r || !r->buf || r->cap < 2 || r->len >= r->cap - 1) return;
    va_start(ap, fmt);
    n = vsnprintf(r->buf + r->len, (size_t)(r->cap - r->len), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= r->cap - r->len) r->len = r->cap - 1; else r->len += n;
}

/* ------------------------------------------------------------------ narzędzia */

static int ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return 0;
    }
    return *a == *b;
}

static int icontains(const char *hay, const char *needle)
{
    size_t nl = strlen(needle), hl = strlen(hay), i, k;
    if (nl == 0) return 1;
    if (hl < nl) return 0;
    for (i = 0; i + nl <= hl; i++) {
        for (k = 0; k < nl; k++) {
            char x = hay[i + k], y = needle[k];
            if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
            if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
            if (x != y) break;
        }
        if (k == nl) return 1;
    }
    return 0;
}

static void scopy(char *dst, size_t n, const char *src)
{
    size_t l;
    if (n == 0) return;
    if (!src) { dst[0] = 0; return; }
    l = strlen(src);
    if (l >= n) l = n - 1;
    memcpy(dst, src, l);
    dst[l] = 0;
}

static long long random63(void)
{
    unsigned char b[8];
    unsigned long long v = 0;
    int i, got = 0;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) { got = (int)fread(b, 1, 8, f) == 8; fclose(f); }
    if (!got) {
        static unsigned s;
        if (!s) s = (unsigned)time(NULL) ^ (unsigned)getpid();
        for (i = 0; i < 8; i++) { s = s * 1103515245u + 12345u; b[i] = (unsigned char)(s >> 16); }
    }
    for (i = 0; i < 8; i++) v = (v << 8) | b[i];
    v &= 0x7FFFFFFFFFFFFFFFULL;
    if (v == 0) v = 1;
    return (long long)v;
}

static unsigned random32(void) { return (unsigned)(random63() & 0xFFFFFFFFLL); }

static int file_exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static int dir_exists(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }

static int mkdir_p(const char *path)
{
    char tmp[1024];
    size_t i, l = strlen(path);
    if (l == 0 || l >= sizeof tmp) return 0;
    memcpy(tmp, path, l + 1);
    for (i = 1; i < l; i++) {
        if (tmp[i] == '/') {
            tmp[i] = 0;
            if (!dir_exists(tmp)) mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
    if (!dir_exists(tmp)) mkdir(tmp, 0755);
    return dir_exists(tmp);
}

static int copy_file(const char *src, const char *dst, rep_t *r)
{
    FILE *in = fopen(src, "rb"), *out;
    unsigned char *buf;
    size_t n;
    long long total = 0;
    int ok = 1;
    if (!in) { rep(r, "  błąd: nie mogę otworzyć pliku źródłowego (%s)\n", strerror(errno)); return 0; }
    out = fopen(dst, "wb");
    if (!out) { rep(r, "  błąd: nie mogę utworzyć %s (%s)\n", dst, strerror(errno)); fclose(in); return 0; }
    buf = (unsigned char *)malloc(65536);
    if (!buf) { fclose(in); fclose(out); return 0; }
    while ((n = fread(buf, 1, 65536, in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
        total += (long long)n;
    }
    free(buf);
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    if (!ok) { rep(r, "  błąd zapisu pliku (brak miejsca?)\n"); remove(dst); return 0; }
    chmod(dst, 0644);
    return 1;
}

/* ------------------------------------------------------------------ SQLite: otwieranie */

static int cmp_nocase(void *u, int la, const void *a, int lb, const void *b)
{
    const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
    int i, n = la < lb ? la : lb;
    (void)u;
    for (i = 0; i < n; i++) {
        int p = x[i], q = y[i];
        if (p >= 'A' && p <= 'Z') p += 32;
        if (q >= 'A' && q <= 'Z') q += 32;
        if (p != q) return p < q ? -1 : 1;
    }
    return la == lb ? 0 : (la < lb ? -1 : 1);
}

static void fn_null(sqlite3_context *c, int argc, sqlite3_value **argv)
{
    if (argc > 0) sqlite3_result_value(c, argv[0]); else sqlite3_result_null(c);
}

/* Jeśli baza używa własnego porównywania napisów albo funkcji, których nie mamy, SQLite zgłasza
   "no such collation sequence: X" / "no such function: X". Rejestrujemy wtedy zastępniki i próbujemy dalej. */
static int fix_missing(sqlite3 *db, const char *msg, rep_t *r)
{
    const char *p;
    char name[64];
    size_t i = 0;
    p = strstr(msg, "no such collation sequence: ");
    if (p) {
        p += strlen("no such collation sequence: ");
        while (*p && *p != ' ' && i < sizeof name - 1) name[i++] = *p++;
        name[i] = 0;
        if (i && sqlite3_create_collation(db, name, SQLITE_UTF8, NULL, cmp_nocase) == SQLITE_OK) {
            rep(r, "  uwaga: baza używa porównywania \"%s\" - użyto zastępnika\n", name);
            return 1;
        }
        return 0;
    }
    p = strstr(msg, "no such function: ");
    if (p) {
        p += strlen("no such function: ");
        while (*p && *p != ' ' && i < sizeof name - 1) name[i++] = *p++;
        name[i] = 0;
        if (i && sqlite3_create_function(db, name, -1, SQLITE_UTF8, NULL, fn_null, NULL, NULL) == SQLITE_OK) {
            rep(r, "  uwaga: baza używa funkcji \"%s\" - użyto zastępnika (zwraca pierwszy argument)\n", name);
            return 1;
        }
    }
    return 0;
}


/* PRAGMA quick_check; 1 = baza w porządku */
static int run_quick_check(sqlite3 *db, rep_t *r, const char *label)
{
    int attempt;
    for (attempt = 0; attempt < 8; attempt++) {
        sqlite3_stmt *st = NULL;
        int rc;
        char msg[240];
        if (sqlite3_prepare_v2(db, "PRAGMA quick_check", -1, &st, NULL) != SQLITE_OK) {
            scopy(msg, sizeof msg, sqlite3_errmsg(db));
            if (st) sqlite3_finalize(st);
            if (fix_missing(db, msg, r)) continue;
            rep(r, "%s: nie udało się uruchomić kontroli (%s)\n", label, msg);
            return 0;
        }
        rc = sqlite3_step(st);
        if (rc == SQLITE_ROW) {
            const char *t = (const char *)sqlite3_column_text(st, 0);
            int ok = t && strcmp(t, "ok") == 0;
            if (!ok) rep(r, "%s: %s\n", label, t ? t : "?");
            sqlite3_finalize(st);
            return ok;
        }
        scopy(msg, sizeof msg, sqlite3_errmsg(db));
        sqlite3_finalize(st);
        if (!fix_missing(db, msg, r)) {
            rep(r, "%s: kontrola nie powiodła się (%s)\n", label, msg);
            return 0;
        }
    }
    return 0;
}

static int open_db(const char *path, sqlite3 **pdb, rep_t *r)
{
    sqlite3 *db = NULL;
    int rc, attempt;
    if (!file_exists(path)) { rep(r, "Nie znaleziono bazy: %s\n", path); return 0; }
    rc = sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK) {
        rep(r, "Nie mogę otworzyć bazy do zapisu (%s): %s\n", path, db ? sqlite3_errmsg(db) : "brak pamięci");
        if (db) sqlite3_close(db);
        return 0;
    }
    sqlite3_busy_timeout(db, 8000);
    for (attempt = 0; attempt < 12; attempt++) {
        char *err = NULL;
        rc = sqlite3_exec(db, "SELECT count(*) FROM sqlite_master", NULL, NULL, &err);
        if (rc == SQLITE_OK) { *pdb = db; return 1; }
        if (!(err && fix_missing(db, err, r))) {
            rep(r, "Nie mogę odczytać schematu bazy: %s\n", err ? err : "?");
            if (err && (strstr(err, "locked") || strstr(err, "busy"))) rep(r, "Baza jest zajęta. Zamknij aplikację Muzyka i spróbuj jeszcze raz.\n");
            sqlite3_free(err);
            sqlite3_close(db);
            return 0;
        }
        sqlite3_free(err);
    }
    sqlite3_close(db);
    rep(r, "Za dużo brakujących elementów bazy\n");
    return 0;
}

static void quote_ident(char *out, size_t n, const char *id)
{
    size_t i = 0;
    if (n < 3) { if (n) out[0] = 0; return; }
    out[i++] = '"';
    while (*id && i + 3 < n) {
        if (*id == '"') out[i++] = '"';
        out[i++] = *id++;
    }
    out[i++] = '"';
    out[i] = 0;
}

/* ------------------------------------------------------------------ tabele i wiersze */

typedef struct { char name[48]; int notnull, has_default, pk, is_text, is_blob; } col_t;
typedef struct { char name[48]; int n; col_t c[MAXC]; } tbl_t;

static int tbl_load(sqlite3 *db, const char *name, tbl_t *t)
{
    char q[160], qi[110];
    sqlite3_stmt *st = NULL;
    memset(t, 0, sizeof *t);
    scopy(t->name, sizeof t->name, name);
    quote_ident(qi, sizeof qi, name);
    snprintf(q, sizeof q, "PRAGMA table_info(%s)", qi);
    if (sqlite3_prepare_v2(db, q, -1, &st, NULL) != SQLITE_OK) return 0;
    while (sqlite3_step(st) == SQLITE_ROW && t->n < MAXC) {
        col_t *c = &t->c[t->n++];
        const char *type = (const char *)sqlite3_column_text(st, 2);
        scopy(c->name, sizeof c->name, (const char *)sqlite3_column_text(st, 1));
        c->notnull = sqlite3_column_int(st, 3);
        c->has_default = sqlite3_column_type(st, 4) != SQLITE_NULL;
        c->pk = sqlite3_column_int(st, 5);
        if (type) {
            c->is_text = icontains(type, "CHAR") || icontains(type, "TEXT") || icontains(type, "CLOB");
            c->is_blob = icontains(type, "BLOB");
        }
    }
    sqlite3_finalize(st);
    return t->n > 0;
}

static int col_index(const tbl_t *t, const char *name)
{
    int i;
    for (i = 0; i < t->n; i++) if (ieq(t->c[i].name, name)) return i;
    return -1;
}

typedef struct { int type; long long i; double d; char *s; unsigned char *b; int n; } val_t;
typedef struct { val_t v[MAXC]; unsigned char set[MAXC]; } row_t;

static void row_init(row_t *r) { memset(r, 0, sizeof *r); }

static void val_free(val_t *v) { free(v->s); free(v->b); memset(v, 0, sizeof *v); }

static void row_free(row_t *r)
{
    int i;
    for (i = 0; i < MAXC; i++) val_free(&r->v[i]);
    memset(r->set, 0, sizeof r->set);
}

/* Czyta pierwszy wiersz pasujący do "where" (z jednym parametrem całkowitym, gdy has_key). 1 = jest, 0 = brak. */
static int row_read(sqlite3 *db, const tbl_t *t, const char *where, int has_key, long long key, row_t *row)
{
    char q[300], qi[110];
    sqlite3_stmt *st = NULL;
    int i, found = 0;
    row_init(row);
    quote_ident(qi, sizeof qi, t->name);
    snprintf(q, sizeof q, "SELECT * FROM %s WHERE %s LIMIT 1", qi, where);
    if (sqlite3_prepare_v2(db, q, -1, &st, NULL) != SQLITE_OK) return 0;
    if (has_key) sqlite3_bind_int64(st, 1, key);
    if (sqlite3_step(st) == SQLITE_ROW) {
        int cols = sqlite3_column_count(st);
        found = 1;
        for (i = 0; i < cols && i < t->n; i++) {
            val_t *v = &row->v[i];
            v->type = sqlite3_column_type(st, i);
            row->set[i] = 1;
            switch (v->type) {
                case SQLITE_INTEGER: v->i = sqlite3_column_int64(st, i); break;
                case SQLITE_FLOAT: v->d = sqlite3_column_double(st, i); break;
                case SQLITE_TEXT: {
                    const char *s = (const char *)sqlite3_column_text(st, i);
                    int l = sqlite3_column_bytes(st, i);
                    v->s = (char *)malloc((size_t)l + 1);
                    if (v->s) { memcpy(v->s, s, (size_t)l); v->s[l] = 0; } else v->type = SQLITE_NULL;
                    break;
                }
                case SQLITE_BLOB: {
                    int l = sqlite3_column_bytes(st, i);
                    const void *b = sqlite3_column_blob(st, i);
                    v->n = l;
                    v->b = (unsigned char *)malloc((size_t)l + 1);
                    if (v->b) { if (l) memcpy(v->b, b, (size_t)l); } else v->type = SQLITE_NULL;
                    break;
                }
                default: v->type = SQLITE_NULL; break;
            }
        }
    }
    sqlite3_finalize(st);
    return found;
}

static void row_copy(row_t *dst, const row_t *src)
{
    int i;
    row_init(dst);
    for (i = 0; i < MAXC; i++) {
        val_t *d = &dst->v[i];
        const val_t *sv = &src->v[i];
        dst->set[i] = src->set[i];
        d->type = sv->type; d->i = sv->i; d->d = sv->d; d->n = sv->n;
        if (sv->s) { d->s = (char *)malloc(strlen(sv->s) + 1); if (d->s) strcpy(d->s, sv->s); }
        if (sv->b) { d->b = (unsigned char *)malloc((size_t)sv->n + 1); if (d->b) memcpy(d->b, sv->b, (size_t)sv->n); }
    }
}

static void set_int(row_t *r, const tbl_t *t, const char *col, long long x)
{
    int i = col_index(t, col);
    if (i < 0) return;
    val_free(&r->v[i]);
    r->v[i].type = SQLITE_INTEGER;
    r->v[i].i = x;
    r->set[i] = 1;
}

static void set_text(row_t *r, const tbl_t *t, const char *col, const char *s)
{
    int i = col_index(t, col);
    if (i < 0) return;
    val_free(&r->v[i]);
    r->v[i].type = SQLITE_TEXT;
    r->v[i].s = (char *)malloc(strlen(s) + 1);
    if (r->v[i].s) strcpy(r->v[i].s, s); else r->v[i].type = SQLITE_NULL;
    r->set[i] = 1;
}

/* NULL, a gdy kolumna nie może być NULL: puste/0 */
static void set_empty(row_t *r, const tbl_t *t, int i)
{
    val_free(&r->v[i]);
    r->set[i] = 1;
    if (!t->c[i].notnull) { r->v[i].type = SQLITE_NULL; return; }
    if (t->c[i].is_text) { r->v[i].type = SQLITE_TEXT; r->v[i].s = (char *)calloc(1, 1); }
    else if (t->c[i].is_blob) { r->v[i].type = SQLITE_BLOB; r->v[i].b = (unsigned char *)calloc(1, 1); r->v[i].n = 0; }
    else { r->v[i].type = SQLITE_INTEGER; r->v[i].i = 0; }
}

static void set_null_col(row_t *r, const tbl_t *t, const char *col)
{
    int i = col_index(t, col);
    if (i >= 0) set_empty(r, t, i);
}

static void set_zero_col(row_t *r, const tbl_t *t, const char *col)
{
    int i = col_index(t, col);
    if (i < 0) return;
    val_free(&r->v[i]);
    r->set[i] = 1;
    if (t->c[i].is_text) { r->v[i].type = SQLITE_TEXT; r->v[i].s = (char *)calloc(1, 1); }
    else { r->v[i].type = SQLITE_INTEGER; r->v[i].i = 0; }
}

/* Dodaje do wiersza wartości dla kolumn NOT NULL bez wartości domyślnej, których nie ustawiliśmy. */
static void row_fill_required(row_t *r, const tbl_t *t)
{
    int i;
    for (i = 0; i < t->n; i++) {
        if (r->set[i]) continue;
        if (t->c[i].notnull && !t->c[i].has_default && !t->c[i].pk) set_empty(r, t, i);
    }
}

/* Z klonowanego wiersza usuwa to, co należy do cudzego utworu: statystyki, zakupy, identyfikatory sklepu... */
static const char *const kReset[] = {
    "play_count", "skip_count", "date_played", "date_skipped", "user_rating", "has_been_played", "bookmark",
    "gapless", "volume_normalization", "start_time", "stop_time", "store_", "download", "purchase", "account",
    "integrity", "genius", "needs_restore", "relative_volume", "eq_preset", "protected", "cloud", "sync_id",
    "feed_url", "podcast", "rental", "last_played", NULL
};

static void row_reset_foreign(row_t *r, const tbl_t *t, const char *keep1)
{
    int i, k;
    for (i = 0; i < t->n; i++) {
        if (!r->set[i]) continue;
        if (keep1 && ieq(t->c[i].name, keep1)) continue;
        for (k = 0; kReset[k]; k++) {
            if (icontains(t->c[i].name, kReset[k])) {
                if (t->c[i].is_text || t->c[i].is_blob || t->c[i].notnull) {
                    if (t->c[i].notnull && !t->c[i].is_text && !t->c[i].is_blob) set_zero_col(r, t, t->c[i].name);
                    else set_empty(r, t, i);
                } else {
                    val_free(&r->v[i]); r->v[i].type = SQLITE_NULL; r->set[i] = 1;
                }
                break;
            }
        }
    }
}

static int row_insert(sqlite3 *db, const tbl_t *t, const row_t *row, rep_t *r, char *errmsg, size_t errn)
{
    char sql[4096], qi[110];
    sqlite3_stmt *st = NULL;
    int i, k = 0, rc;
    size_t len = 0;
    quote_ident(qi, sizeof qi, t->name);
    len += (size_t)snprintf(sql + len, sizeof sql - len, "INSERT INTO %s (", qi);
    for (i = 0; i < t->n; i++) {
        if (!row->set[i]) continue;
        quote_ident(qi, sizeof qi, t->c[i].name);
        len += (size_t)snprintf(sql + len, sizeof sql - len, "%s%s", k ? "," : "", qi);
        k++;
        if (len >= sizeof sql - 200) break;
    }
    len += (size_t)snprintf(sql + len, sizeof sql - len, ") VALUES (");
    for (i = 0; i < k; i++) {
        len += (size_t)snprintf(sql + len, sizeof sql - len, "%s?", i ? "," : "");
        if (len >= sizeof sql - 20) break;
    }
    snprintf(sql + len, sizeof sql - len, ")");
    if (k == 0) { snprintf(errmsg, errn, "%s: brak pól do zapisu", t->name); return 0; }
    rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) { snprintf(errmsg, errn, "%s: %s", t->name, sqlite3_errmsg(db)); return 0; }
    k = 0;
    for (i = 0; i < t->n; i++) {
        const val_t *v;
        if (!row->set[i]) continue;
        v = &row->v[i];
        k++;
        switch (v->type) {
            case SQLITE_INTEGER: sqlite3_bind_int64(st, k, v->i); break;
            case SQLITE_FLOAT: sqlite3_bind_double(st, k, v->d); break;
            case SQLITE_TEXT: sqlite3_bind_text(st, k, v->s ? v->s : "", -1, SQLITE_TRANSIENT); break;
            case SQLITE_BLOB: sqlite3_bind_blob(st, k, v->b ? (const void *)v->b : (const void *)"", v->n, SQLITE_TRANSIENT); break;
            default: sqlite3_bind_null(st, k); break;
        }
    }
    rc = sqlite3_step(st);
    if (rc != SQLITE_DONE) snprintf(errmsg, errn, "%s: %s", t->name, sqlite3_errmsg(db));
    sqlite3_finalize(st);
    (void)r;
    return rc == SQLITE_DONE;
}

/* ------------------------------------------------------------------ pomocnicze zapytania */

static long long query_int(sqlite3 *db, const char *sql, int *found)
{
    sqlite3_stmt *st = NULL;
    long long v = 0;
    if (found) *found = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
    if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_type(st, 0) != SQLITE_NULL) {
        v = sqlite3_column_int64(st, 0);
        if (found) *found = 1;
    }
    sqlite3_finalize(st);
    return v;
}

static int table_exists(sqlite3 *db, const char *name)
{
    sqlite3_stmt *st = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    ok = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return ok;
}

static int is_virtual_table(sqlite3 *db, const char *name)
{
    sqlite3_stmt *st = NULL;
    int v = 0;
    if (sqlite3_prepare_v2(db, "SELECT sql FROM sqlite_master WHERE type='table' AND name=?1", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *s = (const char *)sqlite3_column_text(st, 0);
        v = s && icontains(s, "VIRTUAL TABLE");
    }
    sqlite3_finalize(st);
    return v;
}

/* sortowanie: tabela sort_map (name, name_order, name_section) */
static int sort_info(sqlite3 *db, const tbl_t *sm, const char *name, long long *order, long long *section)
{
    static const char *const queries[] = {
        "SELECT name_order, name_section FROM sort_map WHERE name = ?1 COLLATE NOCASE LIMIT 1",
        "SELECT name_order, name_section FROM sort_map WHERE name < ?1 COLLATE NOCASE ORDER BY name COLLATE NOCASE DESC LIMIT 1",
        "SELECT name_order, name_section FROM sort_map ORDER BY name COLLATE NOCASE ASC LIMIT 1",
        NULL
    };
    int q;
    if (sm->n == 0 || col_index(sm, "name") < 0 || col_index(sm, "name_order") < 0 || col_index(sm, "name_section") < 0) return 0;
    for (q = 0; queries[q]; q++) {
        sqlite3_stmt *st = NULL;
        int ok = 0;
        if (sqlite3_prepare_v2(db, queries[q], -1, &st, NULL) != SQLITE_OK) continue;
        if (q < 2) sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
        if (sqlite3_step(st) == SQLITE_ROW) {
            *order = sqlite3_column_int64(st, 0);
            *section = sqlite3_column_int64(st, 1);
            ok = 1;
        }
        sqlite3_finalize(st);
        if (ok) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ kontekst importu */

typedef struct {
    sqlite3 *db;
    rep_t *r;
    tbl_t item, extra, playback, sortmap;
    int has_item, has_extra, has_playback;
    long long tpl_pid;              /* wzorcowy utwór (0 = brak) */
    row_t tpl_item;
    int have_tpl_item;
    int unix_dates;                 /* daty w bazie liczone od 1970 (a nie od 2001) */
    long long now_db;               /* "teraz" w jednostkach bazy */
    long long base_location_id;
    int have_base_location;
    char media_root[900];
    char music_dir[960];            /* pełna ścieżka folderu Muzyki */
    char music_rel[200];            /* ścieżka folderu Muzyki względem Media */
    char media_root_note[8];
} ctx_t;

static const struct { const char *table, *refcol; } kEnt[] = {
    { "artist", "item_artist_pid" }, { "album_artist", "album_artist_pid" }, { "album", "album_pid" },
    { "genre", "genre_id" }, { "composer", "composer_pid" }
};

static int entity_names(const tbl_t *t, const char **keycol, const char **namecol)
{
    int i;
    *keycol = NULL; *namecol = NULL;
    for (i = 0; i < t->n; i++) if (t->c[i].pk == 1) { *keycol = t->c[i].name; break; }
    if (!*keycol) {
        static const char *const cand[] = { "pid", NULL };
        int k;
        for (k = 0; cand[k]; k++) { int j = col_index(t, cand[k]); if (j >= 0) { *keycol = t->c[j].name; break; } }
    }
    {
        static const char *const cand[] = { "name", "genre", "title", NULL };
        int k;
        for (k = 0; cand[k]; k++) { int j = col_index(t, cand[k]); if (j >= 0) { *namecol = t->c[j].name; break; } }
    }
    return *keycol && *namecol;
}

static int apply_sort_cols(ctx_t *c, row_t *row, const tbl_t *t, const char *col_order, const char *col_section, const char *name)
{
    long long o, s;
    if (!sort_info(c->db, &c->sortmap, name, &o, &s)) return 0;
    set_int(row, t, col_order, o);
    set_int(row, t, col_section, s);
    return 1;
}

/* Znajduje albo tworzy wiersz wykonawcy/albumu/gatunku. Zwraca klucz (0 = brak). */
static long long ensure_entity(ctx_t *c, int which, const char *name, long long album_artist_key, long long new_item_pid,
                               mi_lib_result *res)
{
    const char *table = kEnt[which].table, *refcol = kEnt[which].refcol;
    tbl_t t;
    const char *keycol, *namecol;
    char q[400], qk[110], qn[110], qt[110];
    sqlite3_stmt *st = NULL;
    long long key = 0;
    row_t row;
    int have = 0, tplfound = 0, ti;
    char errmsg[200];

    if (!name || !name[0]) return 0;
    if (!table_exists(c->db, table) || is_virtual_table(c->db, table)) { rep(c->r, "  (brak tabeli %s - pomijam)\n", table); return 0; }
    if (!tbl_load(c->db, table, &t) || !entity_names(&t, &keycol, &namecol)) { rep(c->r, "  (tabela %s ma nieznany układ - pomijam)\n", table); return 0; }
    quote_ident(qk, sizeof qk, keycol);
    quote_ident(qn, sizeof qn, namecol);
    quote_ident(qt, sizeof qt, table);

    /* szukamy istniejącego (album: razem z wykonawcą albumu) */
    if (which == 2 && col_index(&t, "album_artist_pid") >= 0)
        snprintf(q, sizeof q, "SELECT %s FROM %s WHERE %s = ?1 COLLATE NOCASE AND album_artist_pid = ?2 LIMIT 1", qk, qt, qn);
    else
        snprintf(q, sizeof q, "SELECT %s FROM %s WHERE %s = ?1 COLLATE NOCASE LIMIT 1", qk, qt, qn);
    if (sqlite3_prepare_v2(c->db, q, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
        if (which == 2 && col_index(&t, "album_artist_pid") >= 0) sqlite3_bind_int64(st, 2, album_artist_key);
        if (sqlite3_step(st) == SQLITE_ROW) { key = sqlite3_column_int64(st, 0); have = 1; }
        sqlite3_finalize(st);
    }
    if (have) { rep(c->r, "  %s \"%s\": już istnieje (%lld)\n", table, name, key); return key; }

    /* wzór: ten sam rodzaj wiersza, na który wskazuje wzorcowy utwór; w ostateczności dowolny wiersz */
    row_init(&row);
    ti = col_index(&c->item, refcol);
    if (c->have_tpl_item && ti >= 0 && c->tpl_item.set[ti] && c->tpl_item.v[ti].type == SQLITE_INTEGER && c->tpl_item.v[ti].i != 0) {
        char w[200];
        snprintf(w, sizeof w, "%s = ?1", qk);
        tplfound = row_read(c->db, &t, w, 1, c->tpl_item.v[ti].i, &row);
    }
    if (!tplfound) tplfound = row_read(c->db, &t, "1=1", 0, 0, &row);
    if (tplfound) row_reset_foreign(&row, &t, NULL);

    /* nowy klucz, którego jeszcze nie ma */
    {
        int tries;
        for (tries = 0; tries < 20; tries++) {
            int f;
            long long cand = random63();
            snprintf(q, sizeof q, "SELECT 1 FROM %s WHERE %s = %lld", qt, qk, cand);
            query_int(c->db, q, &f);
            if (!f) { key = cand; break; }
        }
    }
    if (!key) { row_free(&row); return 0; }
    set_int(&row, &t, keycol, key);
    set_text(&row, &t, namecol, name);
    if (!apply_sort_cols(c, &row, &t, "name_order", "name_order_section", name))
        apply_sort_cols(c, &row, &t, "name_order", "name_section", name);
    {
        int i;
        for (i = 0; i < t.n; i++) {
            if (icontains(t.c[i].name, "sort_") && t.c[i].is_text) set_empty(&row, &t, i);
        }
    }
    if (which == 2) {
        set_int(&row, &t, "album_artist_pid", album_artist_key);
        set_int(&row, &t, "representative_item_pid", new_item_pid);
    } else {
        set_int(&row, &t, "representative_item_pid", new_item_pid);
    }
    if (!tplfound) row_fill_required(&row, &t);
    if (!row_insert(c->db, &t, &row, c->r, errmsg, sizeof errmsg)) {
        rep(c->r, "  błąd: %s\n", errmsg);
        row_free(&row);
        return -1;
    }
    row_free(&row);
    rep(c->r, "  %s \"%s\": dodano (%lld)%s\n", table, name, key, tplfound ? "" : " [bez wzoru]");
    if (res->ncreated < 5) {
        mi_lib_entity *e = &res->created[res->ncreated++];
        scopy(e->table, sizeof e->table, table);
        scopy(e->keycol, sizeof e->keycol, keycol);
        scopy(e->refcol, sizeof e->refcol, refcol);
        e->key = key;
    }
    return key;
}

/* ------------------------------------------------------------------ przygotowanie kontekstu */

static void prepare_music_dir(ctx_t *c, const char *db_path)
{
    char tmp[900];
    char *s;
    int f = 0;
    scopy(tmp, sizeof tmp, db_path);
    /* <Media>/iTunes_Control/iTunes/MediaLibrary.sqlitedb -> <Media> */
    s = strrchr(tmp, '/'); if (s) *s = 0;
    s = strrchr(tmp, '/'); if (s) *s = 0;
    s = strrchr(tmp, '/'); if (s) *s = 0;
    scopy(c->media_root, sizeof c->media_root, tmp);
    scopy(c->music_rel, sizeof c->music_rel, "iTunes_Control/Music");

    if (table_exists(c->db, "base_location")) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(c->db, "SELECT base_location_id, path FROM base_location WHERE path LIKE '%Music%' ORDER BY base_location_id LIMIT 1", -1, &st, NULL) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) {
                const char *p = (const char *)sqlite3_column_text(st, 1);
                c->base_location_id = sqlite3_column_int64(st, 0);
                c->have_base_location = 1;
                f = 1;
                if (p && p[0]) {
                    if (p[0] == '/') { scopy(c->music_dir, sizeof c->music_dir, p); c->music_rel[0] = 0; }
                    else scopy(c->music_rel, sizeof c->music_rel, p);
                }
            }
            sqlite3_finalize(st);
        }
    }
    if (c->music_rel[0]) snprintf(c->music_dir, sizeof c->music_dir, "%s/%s", c->media_root, c->music_rel);
    if (!dir_exists(c->music_dir)) {
        snprintf(c->music_dir, sizeof c->music_dir, "%s/iTunes_Control/Music", c->media_root);
        if (f) rep(c->r, "uwaga: folder z base_location nie istnieje, używam %s\n", c->music_dir);
    }
}

static void load_template(ctx_t *c, const char *ext)
{
    char q[500];
    int f = 0;
    long long pid = 0;
    if (!c->has_item) return;
    if (c->has_extra && col_index(&c->extra, "location") >= 0 && col_index(&c->extra, "item_pid") >= 0) {
        snprintf(q, sizeof q,
                 "SELECT item.pid FROM item JOIN item_extra ON item_extra.item_pid = item.pid "
                 "WHERE lower(item_extra.location) LIKE '%%.%s' ORDER BY item.pid LIMIT 1", ext);
        pid = query_int(c->db, q, &f);
        if (!f) {
            pid = query_int(c->db, "SELECT item.pid FROM item JOIN item_extra ON item_extra.item_pid = item.pid "
                                    "WHERE item_extra.location IS NOT NULL AND item_extra.location != '' ORDER BY item.pid LIMIT 1", &f);
            if (f) rep(c->r, "uwaga: w bibliotece nie ma utworu .%s - wzorem jest inny format\n", ext);
        }
    }
    if (!f) pid = query_int(c->db, "SELECT pid FROM item ORDER BY pid LIMIT 1", &f);
    c->tpl_pid = f ? pid : 0;
}

/* ------------------------------------------------------------------ jeden utwór */

static int import_one(ctx_t *c, const mi_lib_track *tr, mi_lib_result *res)
{
    char rel[24], full[1100], ext[8], errmsg[200];
    long long pid = 0, k_artist = 0, k_alb_artist = 0, k_album = 0, k_genre = 0;
    row_t it, ex, pb;
    int ok = 0, tries, copied = 0, have_ex = 0, have_pb = 0;
    const char *albumartist;

    memset(res, 0, sizeof *res);
    scopy(ext, sizeof ext, mi_ext_for_format(tr->format));
    if (!ext[0]) { scopy(res->error, sizeof res->error, "nieznany format"); return 0; }
    row_init(&it); row_init(&ex); row_init(&pb);

    /* nazwa pliku, która jeszcze nie istnieje */
    for (tries = 0; tries < 50; tries++) {
        mi_music_path(random32(), random32(), ext, rel, (int)sizeof rel);
        snprintf(full, sizeof full, "%s/%s", c->music_dir, rel);
        if (!file_exists(full)) break;
        rel[0] = 0;
    }
    if (!rel[0]) { scopy(res->error, sizeof res->error, "nie udało się wybrać nazwy pliku"); return 0; }
    {
        char dir[1100];
        char *sl;
        scopy(dir, sizeof dir, full);
        sl = strrchr(dir, '/'); if (sl) *sl = 0;
        if (!mkdir_p(dir)) { snprintf(res->error, sizeof res->error, "nie mogę utworzyć folderu %s", dir); return 0; }
    }
    rep(c->r, "  plik: %s\n", rel);
    if (!copy_file(tr->src_path, full, c->r)) { scopy(res->error, sizeof res->error, "nie udało się skopiować pliku do folderu Muzyki"); return 0; }
    copied = 1;

    /* nowy identyfikator utworu */
    for (tries = 0; tries < 20; tries++) {
        int f;
        char q[120];
        long long cand = random63();
        snprintf(q, sizeof q, "SELECT 1 FROM item WHERE pid = %lld", cand);
        query_int(c->db, q, &f);
        if (!f) { pid = cand; break; }
    }
    if (!pid) { scopy(res->error, sizeof res->error, "nie udało się wybrać identyfikatora"); goto fail; }

    albumartist = (tr->albumartist && tr->albumartist[0]) ? tr->albumartist : NULL;

    if (sqlite3_exec(c->db, "SAVEPOINT mi_track", NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(res->error, sizeof res->error, "SAVEPOINT: %s", sqlite3_errmsg(c->db));
        goto fail;
    }

    /* wykonawca, wykonawca albumu, album, gatunek */
    k_artist = ensure_entity(c, 0, tr->artist, 0, pid, res);
    if (k_artist < 0) { scopy(res->error, sizeof res->error, "nie udało się dodać wykonawcy"); goto fail_sp; }
    k_alb_artist = ensure_entity(c, 1, albumartist ? albumartist : tr->artist, 0, pid, res);
    if (k_alb_artist < 0) { scopy(res->error, sizeof res->error, "nie udało się dodać wykonawcy albumu"); goto fail_sp; }
    k_album = ensure_entity(c, 2, tr->album, k_alb_artist, pid, res);
    if (k_album < 0) { scopy(res->error, sizeof res->error, "nie udało się dodać albumu"); goto fail_sp; }
    k_genre = ensure_entity(c, 3, tr->genre, 0, pid, res);
    if (k_genre < 0) { scopy(res->error, sizeof res->error, "nie udało się dodać gatunku"); goto fail_sp; }

    /* --- item --- */
    if (c->have_tpl_item) {
        row_copy(&it, &c->tpl_item);
        row_reset_foreign(&it, &c->item, NULL);
    }
    set_int(&it, &c->item, "pid", pid);
    set_int(&it, &c->item, "item_artist_pid", k_artist);
    set_int(&it, &c->item, "album_artist_pid", k_alb_artist);
    set_int(&it, &c->item, "album_pid", k_album);
    set_int(&it, &c->item, "genre_id", k_genre);
    set_int(&it, &c->item, "composer_pid", 0);
    set_int(&it, &c->item, "disc_number", tr->disc);
    set_int(&it, &c->item, "track_number", tr->track);
    set_int(&it, &c->item, "in_my_library", 1);
    set_int(&it, &c->item, "remote_location_id", 0);
    set_int(&it, &c->item, "exclude_from_shuffle", 0);
    set_int(&it, &c->item, "is_compilation", 0);
    set_int(&it, &c->item, "date_added", c->now_db);
    set_int(&it, &c->item, "date_downloaded", c->now_db);
    set_int(&it, &c->item, "keep_local", 0);
    if (c->have_base_location) set_int(&it, &c->item, "base_location_id", c->base_location_id);
    if (!c->have_tpl_item) { set_int(&it, &c->item, "media_type", 8); }
    {
        const char *title = (tr->title && tr->title[0]) ? tr->title : "Bez tytułu";
        apply_sort_cols(c, &it, &c->item, "title_order", "title_order_section", title);
        if (tr->artist && tr->artist[0]) apply_sort_cols(c, &it, &c->item, "item_artist_order", "item_artist_order_section", tr->artist);
        if (tr->album && tr->album[0]) apply_sort_cols(c, &it, &c->item, "album_order", "album_order_section", tr->album);
        if (albumartist || (tr->artist && tr->artist[0]))
            apply_sort_cols(c, &it, &c->item, "album_artist_order", "album_artist_order_section", albumartist ? albumartist : tr->artist);
        if (tr->genre && tr->genre[0]) apply_sort_cols(c, &it, &c->item, "genre_order", "genre_order_section", tr->genre);
    }
    if (!c->have_tpl_item) row_fill_required(&it, &c->item);
    for (tries = 0; tries < 4; tries++) {
        if (row_insert(c->db, &c->item, &it, c->r, errmsg, sizeof errmsg)) break;
        if (!fix_missing(c->db, errmsg, c->r)) { snprintf(res->error, sizeof res->error, "zapis do tabeli item nie powiódł się (%s)", errmsg); goto fail_sp; }
    }
    if (tries == 4) { scopy(res->error, sizeof res->error, "zapis do tabeli item nie powiódł się"); goto fail_sp; }
    rep(c->r, "  item: dodano (pid %lld)\n", pid);

    /* --- item_extra --- */
    if (c->has_extra && col_index(&c->extra, "item_pid") >= 0) {
        const char *title = (tr->title && tr->title[0]) ? tr->title : "Bez tytułu";
        if (c->tpl_pid) have_ex = row_read(c->db, &c->extra, "item_pid = ?1", 1, c->tpl_pid, &ex);
        if (have_ex) row_reset_foreign(&ex, &c->extra, NULL);
        set_int(&ex, &c->extra, "item_pid", pid);
        set_text(&ex, &c->extra, "title", title);
        set_null_col(&ex, &c->extra, "sort_title");
        set_int(&ex, &c->extra, "disc_count", tr->disc_total);
        set_int(&ex, &c->extra, "track_count", tr->track_total);
        set_int(&ex, &c->extra, "total_time_ms", tr->duration_ms);
        set_int(&ex, &c->extra, "year", tr->year);
        set_text(&ex, &c->extra, "location", rel);
        {
            struct stat sb;
            if (stat(full, &sb) == 0) set_int(&ex, &c->extra, "file_size", (long long)sb.st_size);
        }
        set_int(&ex, &c->extra, "date_modified", c->now_db);
        set_int(&ex, &c->extra, "is_user_disabled", 0);
        set_int(&ex, &c->extra, "pending_delete", 0);
        if (!have_ex) { set_int(&ex, &c->extra, "media_kind", 1); row_fill_required(&ex, &c->extra); }
        if (!row_insert(c->db, &c->extra, &ex, c->r, errmsg, sizeof errmsg)) { snprintf(res->error, sizeof res->error, "zapis do item_extra nie powiódł się (%s)", errmsg); goto fail_sp; }
        rep(c->r, "  item_extra: dodano%s\n", have_ex ? "" : " [bez wzoru]");
    } else rep(c->r, "  (brak tabeli item_extra z kolumną item_pid)\n");

    /* --- item_playback --- */
    if (c->has_playback && col_index(&c->playback, "item_pid") >= 0) {
        if (c->tpl_pid) have_pb = row_read(c->db, &c->playback, "item_pid = ?1", 1, c->tpl_pid, &pb);
        if (have_pb) row_reset_foreign(&pb, &c->playback, NULL);
        set_int(&pb, &c->playback, "item_pid", pid);
        set_int(&pb, &c->playback, "duration", tr->duration_ms);
        set_int(&pb, &c->playback, "bit_rate", tr->bitrate_kbps);
        set_int(&pb, &c->playback, "sample_rate", tr->sample_rate);
        set_int(&pb, &c->playback, "has_video", 0);
        if (!have_pb) row_fill_required(&pb, &c->playback);
        if (!row_insert(c->db, &c->playback, &pb, c->r, errmsg, sizeof errmsg)) { snprintf(res->error, sizeof res->error, "zapis do item_playback nie powiódł się (%s)", errmsg); goto fail_sp; }
        rep(c->r, "  item_playback: dodano%s\n", have_pb ? "" : " [bez wzoru]");
    } else rep(c->r, "  (brak tabeli item_playback z kolumną item_pid)\n");

    /* --- pozostałe tabele "per utwór": puste wiersze --- */
    {
        static const char *const others[] = { "item_stats", "item_store", "item_video", NULL };
        int k;
        for (k = 0; others[k]; k++) {
            tbl_t t;
            row_t br;
            char e2[200];
            if (!table_exists(c->db, others[k]) || is_virtual_table(c->db, others[k])) continue;
            if (!tbl_load(c->db, others[k], &t) || col_index(&t, "item_pid") < 0) continue;
            row_init(&br);
            set_int(&br, &t, "item_pid", pid);
            row_fill_required(&br, &t);
            if (!row_insert(c->db, &t, &br, c->r, e2, sizeof e2)) {
                rep(c->r, "  uwaga: %s: %s (pomijam)\n", others[k], e2);
            } else rep(c->r, "  %s: dodano\n", others[k]);
            row_free(&br);
        }
    }

    if (sqlite3_exec(c->db, "RELEASE mi_track", NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(res->error, sizeof res->error, "RELEASE: %s", sqlite3_errmsg(c->db));
        goto fail_sp;
    }
    res->ok = 1;
    res->pid = pid;
    scopy(res->rel, sizeof res->rel, rel);
    ok = 1;
    goto done;

fail_sp:
    sqlite3_exec(c->db, "ROLLBACK TO mi_track", NULL, NULL, NULL);
    sqlite3_exec(c->db, "RELEASE mi_track", NULL, NULL, NULL);
    res->ncreated = 0;
fail:
    if (copied) remove(full);
    rep(c->r, "  BŁĄD: %s\n", res->error);
done:
    row_free(&it); row_free(&ex); row_free(&pb);
    return ok;
}

static void diagnose_db(sqlite3 *db, rep_t *r)
{
    sqlite3_stmt *st = NULL;
    char names[MAXT][48];
    int nt = 0, i, k;
    if (sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW && nt < MAXT) scopy(names[nt++], sizeof names[0], (const char *)sqlite3_column_text(st, 0));
        sqlite3_finalize(st);
    }
    rep(r, "\nTabele (%d):\n", nt);
    for (i = 0; i < nt; i++) {
        tbl_t t;
        char q[200], qi[110];
        int f;
        long long cnt = -1;
        quote_ident(qi, sizeof qi, names[i]);
        if (is_virtual_table(db, names[i])) { rep(r, "- %s [wirtualna]\n", names[i]); continue; }
        snprintf(q, sizeof q, "SELECT count(*) FROM %s", qi);
        cnt = query_int(db, q, &f);
        tbl_load(db, names[i], &t);
        rep(r, "- %s (%lld wierszy):", names[i], f ? cnt : -1LL);
        for (k = 0; k < t.n; k++) rep(r, " %s%s", t.c[k].name, t.c[k].pk ? "*" : "");
        rep(r, "\n");
    }
    /* indeksy i wyzwalacze */
    if (sqlite3_prepare_v2(db, "SELECT type, name FROM sqlite_master WHERE type IN ('trigger') ORDER BY name", -1, &st, NULL) == SQLITE_OK) {
        int any = 0;
        while (sqlite3_step(st) == SQLITE_ROW) { if (!any) rep(r, "\nWyzwalacze:"); any = 1; rep(r, " %s", sqlite3_column_text(st, 1)); }
        if (any) rep(r, "\n");
        sqlite3_finalize(st);
    }
    if (sqlite3_prepare_v2(db, "SELECT sql FROM sqlite_master WHERE sql LIKE '%COLLATE%' AND type='index' LIMIT 6", -1, &st, NULL) == SQLITE_OK) {
        int any = 0;
        while (sqlite3_step(st) == SQLITE_ROW) { if (!any) rep(r, "\nIndeksy z COLLATE:\n"); any = 1; rep(r, "  %.150s\n", sqlite3_column_text(st, 0)); }
        sqlite3_finalize(st);
    }
    /* base_location */
    if (table_exists(db, "base_location") && sqlite3_prepare_v2(db, "SELECT * FROM base_location LIMIT 8", -1, &st, NULL) == SQLITE_OK) {
        rep(r, "\nbase_location:\n");
        while (sqlite3_step(st) == SQLITE_ROW) {
            int cc = sqlite3_column_count(st);
            rep(r, " ");
            for (k = 0; k < cc; k++) rep(r, " %s=%.80s", sqlite3_column_name(st, k), sqlite3_column_text(st, k) ? (const char *)sqlite3_column_text(st, k) : "NULL");
            rep(r, "\n");
        }
        sqlite3_finalize(st);
    }
    /* przykładowy utwór */
    {
        int f;
        long long pid = query_int(db, "SELECT pid FROM item WHERE pid IN (SELECT item_pid FROM item_extra WHERE location LIKE '%.mp3') LIMIT 1", &f);
        if (!f) pid = query_int(db, "SELECT pid FROM item ORDER BY pid LIMIT 1", &f);
        if (f) {
            static const char *const tn[] = { "item", "item_extra", "item_playback", "item_stats", "item_store", NULL };
            rep(r, "\nPrzykładowy utwór pid=%lld:\n", pid);
            for (i = 0; tn[i]; i++) {
                tbl_t t;
                row_t row;
                const char *keycol = ieq(tn[i], "item") ? "pid" : "item_pid";
                char w[80];
                if (!table_exists(db, tn[i]) || !tbl_load(db, tn[i], &t)) continue;
                snprintf(w, sizeof w, "%s = ?1", keycol);
                if (!row_read(db, &t, w, 1, pid, &row)) { rep(r, " %s: (brak wiersza)\n", tn[i]); continue; }
                rep(r, " %s:", tn[i]);
                for (k = 0; k < t.n; k++) {
                    const val_t *v = &row.v[k];
                    switch (v->type) {
                        case SQLITE_INTEGER: rep(r, " %s=%lld", t.c[k].name, v->i); break;
                        case SQLITE_FLOAT: rep(r, " %s=%g", t.c[k].name, v->d); break;
                        case SQLITE_TEXT: rep(r, " %s=\"%.40s\"", t.c[k].name, v->s ? v->s : ""); break;
                        case SQLITE_BLOB: rep(r, " %s=<blob %d>", t.c[k].name, v->n); break;
                        default: rep(r, " %s=NULL", t.c[k].name); break;
                    }
                }
                rep(r, "\n");
                row_free(&row);
            }
        }
    }
}

/* ------------------------------------------------------------------ kopie zapasowe */

static int cmp_str(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

static int list_backups(const char *dir, char names[][64], int max)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0;
    if (!d) return 0;
    while ((e = readdir(d)) != NULL && n < max) {
        if (strncmp(e->d_name, "MediaLibrary-", 13) == 0 && strlen(e->d_name) < 63) {
            scopy(names[n], 64, e->d_name);
            n++;
        }
    }
    closedir(d);
    qsort(names, (size_t)n, 64, cmp_str);
    return n;
}

static int make_backup(const char *db_path, const char *backup_dir, rep_t *r)
{
    char dst[1100], stamp[40];
    time_t t = time(NULL);
    struct tm *tmv = localtime(&t);
    char names[40][64];
    int n, i;
    if (!backup_dir) return 1;
    if (!mkdir_p(backup_dir)) { rep(r, "Nie mogę utworzyć folderu kopii %s\n", backup_dir); return 0; }
    if (tmv) strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", tmv); else scopy(stamp, sizeof stamp, "bez-daty");
    snprintf(dst, sizeof dst, "%s/MediaLibrary-%s.sqlitedb", backup_dir, stamp);
    if (!copy_file(db_path, dst, r)) { rep(r, "Nie udało się zrobić kopii bazy - przerywam.\n"); return 0; }
    rep(r, "Kopia bazy: %s\n", dst);
    n = list_backups(backup_dir, names, 40);
    for (i = 0; i < n - 5; i++) {
        snprintf(dst, sizeof dst, "%s/%s", backup_dir, names[i]);
        remove(dst);
    }
    return 1;
}

/* ------------------------------------------------------------------ import */

int mi_lib_import(const char *db_path, const char *backup_dir, const mi_lib_track *tracks, int n,
                  mi_lib_result *res, char *report, int cap)
{
    rep_t rp, *r = &rp;
    ctx_t *c;
    sqlite3 *db = NULL;
    int i, good = 0, f;
    char *err = NULL;
    time_t now = time(NULL);

    rp.buf = report; rp.cap = cap; rp.len = 0;
    if (report && cap > 0) report[0] = 0;
    for (i = 0; i < n; i++) memset(&res[i], 0, sizeof res[i]);

    rep(r, "Music Impactor - import do biblioteki\nBaza: %s\nSQLite: %s\n", db_path, sqlite3_libversion());
    if (n <= 0) { rep(r, "Brak utworów.\n"); return 0; }
    if (!make_backup(db_path, backup_dir, r)) return -1;
    if (!open_db(db_path, &db, r)) return -1;

    c = (ctx_t *)calloc(1, sizeof *c);
    if (!c) { sqlite3_close(db); return -1; }
    c->db = db; c->r = r;
    row_init(&c->tpl_item);

    if (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, &err) != SQLITE_OK) {
        rep(r, "Baza jest zajęta albo zablokowana (%s). Zamknij aplikację Muzyka i spróbuj jeszcze raz.\n", err ? err : "?");
        sqlite3_free(err);
        sqlite3_close(db); free(c);
        return -1;
    }
    /* sprawdzenie przed zmianami */
    if (!run_quick_check(db, r, "Baza jest uszkodzona albo nie da się jej sprawdzić")) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL); sqlite3_close(db); free(c);
        rep(r, "Przerwano - nic nie zmieniono.\n");
        return -1;
    }

    c->has_item = tbl_load(db, "item", &c->item);
    if (!c->has_item || col_index(&c->item, "pid") < 0) {
        rep(r, "W bazie nie ma tabeli item z kolumną pid - to nie wygląda na bibliotekę Muzyki z iOS 6.\n");
        rep(r, "\nOpis bazy (prześlij go autorowi):\n");
        diagnose_db(db, r);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL); sqlite3_close(db); free(c);
        return -1;
    }
    c->has_extra = tbl_load(db, "item_extra", &c->extra);
    c->has_playback = tbl_load(db, "item_playback", &c->playback);
    tbl_load(db, "sort_map", &c->sortmap);
    rep(r, "Tabele: item (%d kolumn), item_extra (%d), item_playback (%d), sort_map (%d)\n",
        c->item.n, c->extra.n, c->playback.n, c->sortmap.n);

    prepare_music_dir(c, db_path);
    rep(r, "Folder Muzyki: %s\n", c->music_dir);
    if (!dir_exists(c->music_dir) && !mkdir_p(c->music_dir)) {
        rep(r, "Nie ma folderu Muzyki i nie mogę go utworzyć.\n");
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL); sqlite3_close(db); free(c);
        return -1;
    }

    for (i = 0; i < n; i++) {
        const mi_lib_track *tr = &tracks[i];
        const char *ext = mi_ext_for_format(tr->format);
        int fdate;
        rep(r, "\n[%d/%d] %s - %s\n", i + 1, n, tr->artist && tr->artist[0] ? tr->artist : "?", tr->title && tr->title[0] ? tr->title : "?");
        /* wzór dobieramy do rozszerzenia pierwszego utworu danego formatu */
        if (i == 0 || !c->have_tpl_item) {
            if (c->have_tpl_item) row_free(&c->tpl_item);
            c->have_tpl_item = 0;
            load_template(c, ext);
            if (c->tpl_pid) {
                char w[40];
                snprintf(w, sizeof w, "pid = ?1");
                c->have_tpl_item = row_read(db, &c->item, w, 1, c->tpl_pid, &c->tpl_item);
                rep(r, "Wzór: utwór %lld\n", c->tpl_pid);
            } else rep(r, "Biblioteka jest pusta - zapisuję bez wzoru (mniej pewne)\n");
            /* daty: 1970 albo 2001 */
            c->unix_dates = 0;
            fdate = col_index(&c->item, "date_added");
            if (c->have_tpl_item && fdate >= 0 && c->tpl_item.set[fdate] && c->tpl_item.v[fdate].type == SQLITE_INTEGER && c->tpl_item.v[fdate].i > 1000000000LL)
                c->unix_dates = 1;
            else if (c->have_tpl_item && fdate >= 0 && c->tpl_item.set[fdate] && c->tpl_item.v[fdate].type == SQLITE_FLOAT && c->tpl_item.v[fdate].d > 1000000000.0)
                c->unix_dates = 1;
            c->now_db = c->unix_dates ? (long long)now : (long long)now - 978307200LL;
        }
        if (import_one(c, tr, &res[i])) good++;
    }

    /* kontrola po zmianach */
    if (!run_quick_check(db, r, "Kontrola po zmianach")) {
        rep(r, "\nKontrola spójności po zmianach NIE przeszła - wycofuję wszystko.\n");
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        for (i = 0; i < n; i++) {
            if (res[i].ok && res[i].rel[0]) {
                char full[1100];
                snprintf(full, sizeof full, "%s/%s", c->music_dir, res[i].rel);
                remove(full);
            }
            res[i].ok = 0;
            scopy(res[i].error, sizeof res[i].error, "baza po zmianach nie przeszła kontroli spójności (wycofano)");
        }
        if (c->have_tpl_item) row_free(&c->tpl_item);
        sqlite3_close(db); free(c);
        return -1;
    }
    if (good > 0) {
        if (sqlite3_exec(db, "COMMIT", NULL, NULL, &err) != SQLITE_OK) {
            rep(r, "\nNie udało się zatwierdzić zmian: %s\n", err ? err : "?");
            sqlite3_free(err);
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            for (i = 0; i < n; i++) {
                if (res[i].ok && res[i].rel[0]) {
                    char full[1100];
                    snprintf(full, sizeof full, "%s/%s", c->music_dir, res[i].rel);
                    remove(full);
                }
                res[i].ok = 0;
                scopy(res[i].error, sizeof res[i].error, "nie udało się zatwierdzić zmian w bazie");
            }
            sqlite3_close(db); free(c);
            return -1;
        }
        rep(r, "\nZatwierdzono zmiany: %d z %d utworów.\n", good, n);
    } else {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        rep(r, "\nNic nie zapisano.\n");
    }
    f = 0; (void)f;
    if (c->have_tpl_item) row_free(&c->tpl_item);
    sqlite3_close(db);
    free(c);
    return good;
}

/* ------------------------------------------------------------------ usuwanie */

int mi_lib_remove(const char *db_path, long long pid, const char *rel,
                  const mi_lib_entity *ents, int nent, char *report, int cap)
{
    rep_t rp, *r = &rp;
    sqlite3 *db = NULL;
    sqlite3_stmt *st = NULL;
    char names[MAXT][48];
    int nt = 0, i, ok = 0;
    char q[400], qi[110];
    ctx_t c;
    char *err = NULL;

    rp.buf = report; rp.cap = cap; rp.len = 0;
    if (report && cap > 0) report[0] = 0;
    if (!open_db(db_path, &db, r)) return 0;
    memset(&c, 0, sizeof c);
    c.db = db; c.r = r;
    if (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, &err) != SQLITE_OK) {
        rep(r, "Baza jest zajęta (%s). Zamknij Muzykę.\n", err ? err : "?");
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }
    /* wszystkie zwykłe tabele z kolumną item_pid */
    if (sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW && nt < MAXT) {
            scopy(names[nt++], sizeof names[0], (const char *)sqlite3_column_text(st, 0));
        }
        sqlite3_finalize(st);
    }
    for (i = 0; i < nt; i++) {
        tbl_t t;
        if (is_virtual_table(db, names[i])) continue;
        if (!tbl_load(db, names[i], &t)) continue;
        if (col_index(&t, "item_pid") < 0) continue;
        quote_ident(qi, sizeof qi, names[i]);
        snprintf(q, sizeof q, "DELETE FROM %s WHERE item_pid = %lld", qi, pid);
        if (sqlite3_exec(db, q, NULL, NULL, &err) != SQLITE_OK) {
            char msg[200];
            scopy(msg, sizeof msg, err ? err : "");
            sqlite3_free(err); err = NULL;
            if (fix_missing(db, msg, r)) { sqlite3_exec(db, q, NULL, NULL, NULL); }
            else rep(r, "uwaga: %s: %s\n", names[i], msg);
        }
    }
    snprintf(q, sizeof q, "DELETE FROM item WHERE pid = %lld", pid);
    if (sqlite3_exec(db, q, NULL, NULL, &err) != SQLITE_OK) {
        rep(r, "Nie udało się usunąć wiersza z item: %s\n", err ? err : "?");
        sqlite3_free(err);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db);
        return 0;
    }
    /* wykonawcy / albumy / gatunki, które dodaliśmy i nikt już ich nie używa */
    for (i = 0; i < nent; i++) {
        int f;
        char qt[110], qk[110];
        long long cnt;
        tbl_t te;
        if (!ents[i].table[0] || !ents[i].keycol[0] || !ents[i].refcol[0]) continue;
        if (!tbl_load(db, "item", &te) || col_index(&te, ents[i].refcol) < 0) continue;
        quote_ident(qi, sizeof qi, ents[i].refcol);
        snprintf(q, sizeof q, "SELECT count(*) FROM item WHERE %s = %lld", qi, ents[i].key);
        cnt = query_int(db, q, &f);
        if (f && cnt == 0) {
            quote_ident(qt, sizeof qt, ents[i].table);
            quote_ident(qk, sizeof qk, ents[i].keycol);
            snprintf(q, sizeof q, "DELETE FROM %s WHERE %s = %lld", qt, qk, ents[i].key);
            sqlite3_exec(db, q, NULL, NULL, NULL);
            rep(r, "Usunięto z %s niepotrzebny wiersz %lld\n", ents[i].table, ents[i].key);
        }
    }
    if (sqlite3_exec(db, "COMMIT", NULL, NULL, &err) != SQLITE_OK) {
        rep(r, "Nie udało się zatwierdzić: %s\n", err ? err : "?");
        sqlite3_free(err);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db);
        return 0;
    }
    ok = 1;
    if (rel && rel[0]) {
        prepare_music_dir(&c, db_path);
        {
            char full[1100];
            snprintf(full, sizeof full, "%s/%s", c.music_dir, rel);
            if (file_exists(full)) { remove(full); rep(r, "Usunięto plik %s\n", rel); }
        }
    }
    rep(r, "Usunięto utwór %lld z biblioteki.\n", pid);
    sqlite3_close(db);
    return ok;
}

/* ------------------------------------------------------------------ diagnostyka */

int mi_lib_diagnose(const char *db_path, char *out, int cap)
{
    rep_t rp, *r = &rp;
    sqlite3 *db = NULL;
    struct stat sb;

    rp.buf = out; rp.cap = cap; rp.len = 0;
    if (out && cap > 0) out[0] = 0;
    rep(r, "Music Impactor - diagnostyka biblioteki\nBaza: %s\nSQLite: %s\n", db_path, sqlite3_libversion());
    if (stat(db_path, &sb) == 0) rep(r, "Rozmiar pliku: %ld B\n", (long)sb.st_size);
    if (!open_db(db_path, &db, r)) return 0;
    diagnose_db(db, r);
    sqlite3_close(db);
    return 1;
}

/* ------------------------------------------------------------------ przywracanie kopii */

int mi_lib_restore_backup(const char *db_path, const char *backup_dir, char *report, int cap)
{
    rep_t rp, *r = &rp;
    char names[40][64], src[1100], tmp[1100];
    int n;
    rp.buf = report; rp.cap = cap; rp.len = 0;
    if (report && cap > 0) report[0] = 0;
    n = list_backups(backup_dir, names, 40);
    if (n == 0) { rep(r, "Nie ma żadnej kopii w %s\n", backup_dir); return 0; }
    snprintf(src, sizeof src, "%s/%s", backup_dir, names[n - 1]);
    snprintf(tmp, sizeof tmp, "%s.mi-przywracanie", db_path);
    if (!copy_file(src, tmp, r)) return 0;
    {
        char j[1100];
        snprintf(j, sizeof j, "%s-journal", db_path); remove(j);
        snprintf(j, sizeof j, "%s-wal", db_path); remove(j);
        snprintf(j, sizeof j, "%s-shm", db_path); remove(j);
    }
    if (rename(tmp, db_path) != 0) {
        rep(r, "Nie udało się podmienić bazy (%s)\n", strerror(errno));
        remove(tmp);
        return 0;
    }
    rep(r, "Przywrócono kopię %s\n", names[n - 1]);
    return 1;
}
