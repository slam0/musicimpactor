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
    int ok = 1;
    if (!in) { rep(r, "  błąd: nie mogę otworzyć pliku źródłowego (%s)\n", strerror(errno)); return 0; }
    out = fopen(dst, "wb");
    if (!out) { rep(r, "  błąd: nie mogę utworzyć %s (%s)\n", dst, strerror(errno)); fclose(in); return 0; }
    buf = (unsigned char *)malloc(65536);
    if (!buf) { fclose(in); fclose(out); return 0; }
    while ((n = fread(buf, 1, 65536, in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
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

typedef struct { char name[48]; char type[20]; int notnull, has_default, pk, is_text, is_blob; } col_t;
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
            scopy(c->type, sizeof c->type, type);
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

/* Dodaje do wiersza wartości dla kolumn NOT NULL bez wartości domyślnej, których nie ustawiliśmy. */
static void row_fill_required(row_t *r, const tbl_t *t)
{
    int i;
    for (i = 0; i < t->n; i++) {
        if (r->set[i]) continue;
        if (t->c[i].notnull && !t->c[i].has_default && !t->c[i].pk) set_empty(r, t, i);
    }
}

/* Z klonowanego wiersza usuwa to, co należy do cudzego utworu: statystyki, zakupy, okładki, identyfikatory sklepu... */
static const char *const kReset[] = {
    "play_count", "skip_count", "date_played", "date_skipped", "user_rating", "has_been_played", "bookmark",
    "gapless", "volume_normalization", "start_time", "stop_time", "store_", "download", "purchase", "account",
    "integrity", "genius", "needs_restore", "relative_volume", "eq_preset", "protected", "cloud", "sync_id",
    "feed_url", "podcast", "rental", "last_played", "artwork", "grouping", "comment", "description", "copyright",
    "external_guid", "audio_fingerprint", "key_", "date_released", "chosen_by", "is_ota",
    "total_burn", "collection_", NULL
};

static int reset_name_match(const char *name)
{
    int k;
    for (k = 0; kReset[k]; k++) if (icontains(name, kReset[k])) return 1;
    return 0;
}

/* tekst/blob -> NULL (albo puste, gdy kolumna nie może być NULL); liczba -> 0 (tylko gdy we wzorze coś było) */
static void row_reset_foreign(row_t *r, const tbl_t *t, const char *keep1)
{
    int i;
    for (i = 0; i < t->n; i++) {
        if (!r->set[i]) continue;
        if (keep1 && ieq(t->c[i].name, keep1)) continue;
        if (!reset_name_match(t->c[i].name)) continue;
        if (r->v[i].type == SQLITE_NULL) continue;
        if (r->v[i].type == SQLITE_TEXT || r->v[i].type == SQLITE_BLOB) set_empty(r, t, i);
        else { val_free(&r->v[i]); r->v[i].type = SQLITE_INTEGER; r->v[i].i = 0; r->set[i] = 1; }
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

/* ------------------------------------------------------------------ pomocnicze zapisy wartości */

static void set_real(row_t *r, const tbl_t *t, const char *col, double x)
{
    int i = col_index(t, col);
    if (i < 0) return;
    val_free(&r->v[i]);
    r->v[i].type = SQLITE_FLOAT;
    r->v[i].d = x;
    r->set[i] = 1;
}

static void set_bytes(row_t *r, const tbl_t *t, const char *col, const void *data, int n)
{
    int i = col_index(t, col);
    if (i < 0) return;
    val_free(&r->v[i]);
    r->v[i].type = SQLITE_BLOB;
    r->v[i].b = (unsigned char *)malloc((size_t)n + 1);
    if (r->v[i].b) { if (n) memcpy(r->v[i].b, data, (size_t)n); r->v[i].n = n; }
    else r->v[i].type = SQLITE_NULL;
    r->set[i] = 1;
}

/* wartość liczbowa jako liczba całkowita (bez konwersji double <-> 64 bity: tych funkcji nie ma w stubie systemu) */
static long long val_ll(const val_t *v)
{
    if (v->type == SQLITE_INTEGER) return v->i;
    if (v->type == SQLITE_FLOAT) {
        if (v->d >= 2147483000.0) return 2147483000LL;
        if (v->d <= -2147483000.0) return -2147483000LL;
        return (long long)(int)v->d;
    }
    return 0;
}

static int has_upper(const char *s)
{
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') return 1;
    return 0;
}

static int has_letter(const char *s)
{
    for (; *s; s++) if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')) return 1;
    return 0;
}

static void lower_ascii(char *d, size_t n, const char *s)
{
    size_t i = 0;
    while (s[i] && i + 1 < n) { d[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] + 32) : s[i]; i++; }
    d[i] = 0;
}

/* ------------------------------------------------------------------ kontekst importu */

#define MAXBL 64

typedef struct { long long id; char path[110]; } bl_t;

typedef struct {
    sqlite3 *db;
    rep_t *r;
    tbl_t item, extra, sortmap;
    char ikey[24];                  /* kolumna klucza w tabeli item: item_pid */
    int has_item, has_extra;
    long long tpl_pid;              /* wzorcowy utwór (0 = brak) */
    row_t tpl_item;
    int have_tpl_item;
    int tpl_loaded;
    char tpl_ext[8];
    int unix_dates;                 /* daty w bazie liczone od 1970 (a nie od 2001) */
    long long now_db;               /* "teraz" w jednostkach bazy */
    bl_t bl[MAXBL];                 /* wiersze base_location z folderami Muzyki */
    int nbl;
    int fmode;                      /* 1 = osobny wiersz base_location dla każdego folderu Fxx (iOS 6) */
    int have_rev;                   /* jest tabela entity_revision */
    long long rev_old_max, rev_max;
    char ptab[24][48];              /* tabele "per utwór" poza item, item_extra, item_search */
    int nptab;
    char media_root[900];
    char music_dir[960];            /* pełna ścieżka folderu, w którym leżą foldery Fxx */
    char music_rel[200];            /* ta sama ścieżka względem Media */
} ctx_t;

typedef struct { const char *table, *alt, *refcol, *ordprefix; } entkind_t;

static const entkind_t kEnt[] = {
    { "item_artist", "artist", "item_artist_pid", "item_artist" },
    { "album_artist", NULL, "album_artist_pid", "album_artist" },
    { "album", NULL, "album_pid", "album" },
    { "genre", NULL, "genre_id", "genre" },
    { "composer", NULL, "composer_pid", "composer" }
};

static const char *resolve_table(sqlite3 *db, const entkind_t *k)
{
    if (table_exists(db, k->table) && !is_virtual_table(db, k->table)) return k->table;
    if (k->alt && table_exists(db, k->alt) && !is_virtual_table(db, k->alt)) return k->alt;
    return NULL;
}

static int entity_names(const tbl_t *t, const char **keycol, const char **namecol)
{
    int i, k;
    const char *cand[5];
    *keycol = NULL; *namecol = NULL;
    for (i = 0; i < t->n; i++) if (t->c[i].pk == 1) { *keycol = t->c[i].name; break; }
    if (!*keycol && t->n > 0) *keycol = t->c[0].name;
    cand[0] = t->name; cand[1] = "name"; cand[2] = "genre"; cand[3] = "title"; cand[4] = NULL;
    for (k = 0; cand[k]; k++) {
        int j = col_index(t, cand[k]);
        if (j >= 0 && !(*keycol && ieq(t->c[j].name, *keycol))) { *namecol = t->c[j].name; break; }
    }
    return *keycol && *namecol;
}

/* sortowanie: kolumny <prefix>_order i <prefix>_order_section w wierszu utworu */
static void set_order(ctx_t *c, row_t *row, const tbl_t *t, const char *prefix, const char *name)
{
    char c1[64], c2[64];
    long long o = 0, s = 0;
    snprintf(c1, sizeof c1, "%s_order", prefix);
    snprintf(c2, sizeof c2, "%s_order_section", prefix);
    if (col_index(t, c1) < 0) return;
    if (!sort_info(c->db, &c->sortmap, name ? name : "", &o, &s)) { o = 0; s = 0; }
    set_int(row, t, c1, o);
    set_int(row, t, c2, s);
}

/* ------------------------------------------------------------------ dziennik zmian (entity_revision) */

static void rev_add(ctx_t *c, long long entity_pid, const char *sample_table, const char *sample_key)
{
    sqlite3_stmt *st = NULL, *ins = NULL;
    char q[500], qt[110], qk[110];
    if (!c->have_rev) return;
    quote_ident(qt, sizeof qt, sample_table);
    quote_ident(qk, sizeof qk, sample_key);
    snprintf(q, sizeof q,
             "SELECT class, revision_type FROM entity_revision WHERE entity_pid IN (SELECT %s FROM %s) ORDER BY revision DESC LIMIT 1", qk, qt);
    if (sqlite3_prepare_v2(c->db, q, -1, &st, NULL) != SQLITE_OK) return;
    if (sqlite3_step(st) == SQLITE_ROW) {
        if (sqlite3_prepare_v2(c->db, "INSERT INTO entity_revision (revision, entity_pid, deleted, class, revision_type) VALUES (?1, ?2, 0, ?3, ?4)",
                               -1, &ins, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(ins, 1, c->rev_max + 1);
            sqlite3_bind_int64(ins, 2, entity_pid);
            sqlite3_bind_value(ins, 3, sqlite3_column_value(st, 0));
            sqlite3_bind_value(ins, 4, sqlite3_column_value(st, 1));
            if (sqlite3_step(ins) == SQLITE_DONE) c->rev_max++;
            sqlite3_finalize(ins);
        }
    }
    sqlite3_finalize(st);
}

/* ------------------------------------------------------------------ wykonawca, album, gatunek */

/* Znajduje albo tworzy wiersz wykonawcy/albumu/gatunku. Zwraca klucz (0 = brak, -1 = błąd). */
static long long ensure_entity(ctx_t *c, int which, const char *name, long long album_artist_key, long long new_item_pid,
                               mi_lib_result *res)
{
    const entkind_t *kind = &kEnt[which];
    const char *table = resolve_table(c->db, kind);
    const char *refcol = kind->refcol;
    tbl_t t;
    const char *keycol, *namecol;
    char q[500], qk[110], qn[110], qt[110];
    sqlite3_stmt *st = NULL;
    long long key = 0;
    row_t row;
    int have = 0, tplfound = 0, ti, i, gk_blob = 0, is_album;
    char errmsg[200];

    if (!name) name = "";
    if (!table) { if (name[0]) rep(c->r, "  (brak tabeli %s - pomijam)\n", kind->table); return 0; }
    if (!tbl_load(c->db, table, &t) || !entity_names(&t, &keycol, &namecol)) { rep(c->r, "  (tabela %s ma nieznany układ - pomijam)\n", table); return 0; }
    quote_ident(qk, sizeof qk, keycol);
    quote_ident(qn, sizeof qn, namecol);
    quote_ident(qt, sizeof qt, table);
    is_album = which == 2 && col_index(&t, "album_artist_pid") >= 0;

    /* szukamy istniejącego (album: razem z wykonawcą albumu) */
    if (is_album)
        snprintf(q, sizeof q, "SELECT %s FROM %s WHERE %s = ?1 COLLATE NOCASE AND album_artist_pid = ?2 LIMIT 1", qk, qt, qn);
    else
        snprintf(q, sizeof q, "SELECT %s FROM %s WHERE %s = ?1 COLLATE NOCASE LIMIT 1", qk, qt, qn);
    if (sqlite3_prepare_v2(c->db, q, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
        if (is_album) sqlite3_bind_int64(st, 2, album_artist_key);
        if (sqlite3_step(st) == SQLITE_ROW) { key = sqlite3_column_int64(st, 0); have = 1; }
        sqlite3_finalize(st);
    }
    if (have) { rep(c->r, "  %s \"%s\": już istnieje (%lld)\n", table, name, key); return key; }
    /* bez nazwy: używamy pustego wiersza, jeśli baza go ma; inaczej 0 (brak) */
    if (!name[0]) return 0;

    /* wzór: ten sam rodzaj wiersza, na który wskazuje wzorcowy utwór; w ostateczności dowolny wiersz */
    row_init(&row);
    ti = col_index(&c->item, refcol);
    if (c->have_tpl_item && ti >= 0 && c->tpl_item.set[ti] && c->tpl_item.v[ti].type == SQLITE_INTEGER && c->tpl_item.v[ti].i != 0) {
        char w[200];
        snprintf(w, sizeof w, "%s = ?1", qk);
        tplfound = row_read(c->db, &t, w, 1, c->tpl_item.v[ti].i, &row);
    }
    if (!tplfound) tplfound = row_read(c->db, &t, "1=1", 0, 0, &row);
    i = col_index(&t, "grouping_key");
    if (i >= 0) gk_blob = t.c[i].is_blob || (row.set[i] && row.v[i].type == SQLITE_BLOB);
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
    for (i = 0; i < t.n; i++) {
        if (icontains(t.c[i].name, "sort_") && t.c[i].is_text) set_empty(&row, &t, i);
    }
    /* klucz grupujący: musi być inny dla każdej nazwy (we wzorze jest cudzy) */
    if (col_index(&t, "grouping_key") >= 0) {
        char g[300], low[220];
        lower_ascii(low, sizeof low, name);
        if (is_album) snprintf(g, sizeof g, "%s\x01%lld", low, album_artist_key);
        else snprintf(g, sizeof g, "%s", low);
        if (gk_blob) set_bytes(&row, &t, "grouping_key", g, (int)strlen(g));
        else set_text(&row, &t, "grouping_key", g);
    }
    if (is_album) set_int(&row, &t, "album_artist_pid", album_artist_key);
    set_int(&row, &t, "representative_item_pid", new_item_pid);
    if (!tplfound) row_fill_required(&row, &t);
    if (!row_insert(c->db, &t, &row, c->r, errmsg, sizeof errmsg)) {
        rep(c->r, "  błąd: %s\n", errmsg);
        row_free(&row);
        return -1;
    }
    row_free(&row);
    rep(c->r, "  %s \"%s\": dodano (%lld)%s\n", table, name, key, tplfound ? "" : " [bez wzoru]");
    rev_add(c, key, table, keycol);
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

static int is_fdir(const char *s)
{
    return (s[0] == 'F' || s[0] == 'f') && s[1] >= '0' && s[1] <= '9' && s[2] >= '0' && s[2] <= '9' && s[3] == 0;
}

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
    c->nbl = 0; c->fmode = 0;

    if (table_exists(c->db, "base_location")) {
        sqlite3_stmt *st = NULL;
        bl_t legacy;
        int have_legacy = 0;
        memset(&legacy, 0, sizeof legacy);
        if (sqlite3_prepare_v2(c->db, "SELECT base_location_id, path FROM base_location ORDER BY base_location_id", -1, &st, NULL) == SQLITE_OK) {
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char *p = (const char *)sqlite3_column_text(st, 1);
                const char *last;
                if (!p || !p[0] || !icontains(p, "Music")) continue;
                last = strrchr(p, '/');
                last = last ? last + 1 : p;
                if (is_fdir(last)) {
                    if (c->nbl < MAXBL) {
                        c->bl[c->nbl].id = sqlite3_column_int64(st, 0);
                        scopy(c->bl[c->nbl].path, sizeof c->bl[0].path, p);
                        c->nbl++;
                        c->fmode = 1;
                    }
                } else if (!have_legacy) {
                    legacy.id = sqlite3_column_int64(st, 0);
                    scopy(legacy.path, sizeof legacy.path, p);
                    have_legacy = 1;
                }
            }
            sqlite3_finalize(st);
        }
        if (c->fmode) {
            char *sl;
            scopy(tmp, sizeof tmp, c->bl[0].path);
            sl = strrchr(tmp, '/');
            if (sl) { *sl = 0; scopy(c->music_rel, sizeof c->music_rel, tmp); } else c->music_rel[0] = 0;
            f = 1;
        } else if (have_legacy) {
            c->bl[0] = legacy; c->nbl = 1; f = 1;
            scopy(c->music_rel, sizeof c->music_rel, legacy.path);
        }
    }
    if (c->music_rel[0] == '/') { scopy(c->music_dir, sizeof c->music_dir, c->music_rel); c->music_rel[0] = 0; }
    else if (c->music_rel[0]) snprintf(c->music_dir, sizeof c->music_dir, "%s/%s", c->media_root, c->music_rel);
    else scopy(c->music_dir, sizeof c->music_dir, c->media_root);
    if (!dir_exists(c->music_dir) && !c->fmode) {
        snprintf(c->music_dir, sizeof c->music_dir, "%s/iTunes_Control/Music", c->media_root);
        if (f) rep(c->r, "uwaga: folder z base_location nie istnieje, używam %s\n", c->music_dir);
    }
}

/* pełna ścieżka folderu dla wiersza base_location */
static void bl_dir(const ctx_t *c, int idx, char *out, size_t n)
{
    if (c->bl[idx].path[0] == '/') scopy(out, n, c->bl[idx].path);
    else snprintf(out, n, "%s/%s", c->media_root, c->bl[idx].path);
}

/* losowy folder Fxx; jeśli taki folder już istnieje na dysku, wybieramy istniejący */
static int pick_folder(const ctx_t *c)
{
    unsigned start = (unsigned)(((unsigned long long)random32() * (unsigned)c->nbl) >> 32);
    int i;
    char dir[1100];
    for (i = 0; i < c->nbl; i++) {
        int idx = (int)start + i;
        if (idx >= c->nbl) idx -= c->nbl;
        bl_dir(c, idx, dir, sizeof dir);
        if (dir_exists(dir)) return idx;
    }
    return (int)start;
}

static void load_template(ctx_t *c, const char *ext)
{
    char q[600];
    int f = 0;
    long long pid = 0;
    if (!c->has_item) return;
    if (c->has_extra && col_index(&c->extra, "location") >= 0 && col_index(&c->extra, "item_pid") >= 0) {
        snprintf(q, sizeof q,
                 "SELECT item.%s FROM item JOIN item_extra ON item_extra.item_pid = item.%s "
                 "WHERE lower(item_extra.location) LIKE '%%.%s' ORDER BY item.%s LIMIT 1", c->ikey, c->ikey, ext, c->ikey);
        pid = query_int(c->db, q, &f);
        if (!f) {
            snprintf(q, sizeof q,
                     "SELECT item.%s FROM item JOIN item_extra ON item_extra.item_pid = item.%s "
                     "WHERE item_extra.location IS NOT NULL AND item_extra.location != '' ORDER BY item.%s LIMIT 1", c->ikey, c->ikey, c->ikey);
            pid = query_int(c->db, q, &f);
            if (f) rep(c->r, "uwaga: w bibliotece nie ma utworu .%s - wzorem jest inny format (kody formatu mogą się nie zgadzać)\n", ext);
        }
    }
    if (!f) {
        snprintf(q, sizeof q, "SELECT %s FROM item ORDER BY %s LIMIT 1", c->ikey, c->ikey);
        pid = query_int(c->db, q, &f);
    }
    c->tpl_pid = f ? pid : 0;
}

/* Skąd liczone są daty: sprawdzamy wzorcowy utwór (item_extra.date_created / date_modified). */
static void detect_dates(ctx_t *c, time_t now)
{
    static const char *const cols[] = { "date_created", "date_modified", NULL };
    int k;
    c->unix_dates = 0;
    if (c->tpl_pid && c->has_extra) {
        for (k = 0; cols[k]; k++) {
            char q[200];
            int f;
            long long v;
            if (col_index(&c->extra, cols[k]) < 0) continue;
            snprintf(q, sizeof q, "SELECT %s FROM item_extra WHERE item_pid = %lld", cols[k], c->tpl_pid);
            v = query_int(c->db, q, &f);
            if (f && v > 0) { c->unix_dates = v > 1000000000LL; break; }
        }
    }
    c->now_db = c->unix_dates ? (long long)now : (long long)now - 978307200LL;
}

/* ------------------------------------------------------------------ item_search */

static int add_search(ctx_t *c, long long pid, const char *title, const char *artist, const char *album,
                      const char *albumartist, mi_lib_result *res)
{
    static const char *const cols[5] = { "search_title", "search_album", "search_artist", "search_composer", "search_album_artist" };
    tbl_t t;
    row_t tp, nr;
    int have = 0, k, lower = 0, seen_letter = 0, seen_upper = 0;
    char errmsg[200], low[600];
    const char *vals[5];
    vals[0] = title; vals[1] = album; vals[2] = artist; vals[3] = ""; vals[4] = albumartist;
    if (!table_exists(c->db, "item_search") || is_virtual_table(c->db, "item_search")) return 1;
    if (!tbl_load(c->db, "item_search", &t) || col_index(&t, "item_pid") < 0) return 1;
    row_init(&tp); row_init(&nr);
    if (c->tpl_pid) have = row_read(c->db, &t, "item_pid = ?1", 1, c->tpl_pid, &tp);
    if (have) {
        for (k = 0; k < 5; k++) {
            int i = col_index(&t, cols[k]);
            if (i >= 0 && tp.v[i].type == SQLITE_TEXT && tp.v[i].s) {
                if (has_letter(tp.v[i].s)) seen_letter = 1;
                if (has_upper(tp.v[i].s)) seen_upper = 1;
            }
        }
        lower = seen_letter && !seen_upper;
    }
    set_int(&nr, &t, "item_pid", pid);
    for (k = 0; k < 5; k++) {
        int i = col_index(&t, cols[k]);
        int text_mode;
        if (i < 0) continue;
        text_mode = have ? (tp.v[i].type == SQLITE_TEXT || (tp.v[i].type == SQLITE_NULL && t.c[i].is_text)) : t.c[i].is_text;
        if (text_mode) {
            if (lower) { lower_ascii(low, sizeof low, vals[k] ? vals[k] : ""); set_text(&nr, &t, cols[k], low); }
            else set_text(&nr, &t, cols[k], vals[k] ? vals[k] : "");
        } else set_int(&nr, &t, cols[k], 0);
    }
    row_fill_required(&nr, &t);
    if (!row_insert(c->db, &t, &nr, c->r, errmsg, sizeof errmsg)) {
        snprintf(res->error, sizeof res->error, "zapis do item_search nie powiódł się (%s)", errmsg);
        row_free(&tp); row_free(&nr);
        return 0;
    }
    rep(c->r, "  item_search: dodano%s\n", have ? "" : " [bez wzoru]");
    row_free(&tp); row_free(&nr);
    return 1;
}

/* ------------------------------------------------------------------ jeden utwór */

static int import_one(ctx_t *c, const mi_lib_track *tr, mi_lib_result *res)
{
    char rel[24], fname[16], full[1100], ext[8], errmsg[200];
    long long pid = 0, k_artist = 0, k_alb_artist = 0, k_album = 0, k_genre = 0, k_composer = 0, base_id = 0;
    row_t it, ex;
    int ok = 0, tries, copied = 0, have_ex = 0, bi = -1, k;
    const char *albumartist, *title;

    memset(res, 0, sizeof *res);
    scopy(ext, sizeof ext, mi_ext_for_format(tr->format));
    if (!ext[0]) { scopy(res->error, sizeof res->error, "nieznany format"); return 0; }
    row_init(&it); row_init(&ex);
    title = (tr->title && tr->title[0]) ? tr->title : "Bez tytułu";

    /* folder i nazwa pliku, która jeszcze nie istnieje */
    if (c->fmode && c->nbl > 0) bi = pick_folder(c);
    for (tries = 0; tries < 50; tries++) {
        char tmpname[24], dir[1100];
        const char *sl;
        mi_music_path(random32(), random32(), ext, tmpname, (int)sizeof tmpname);
        sl = strchr(tmpname, '/');
        scopy(fname, sizeof fname, sl ? sl + 1 : tmpname);
        if (bi >= 0) {
            const char *last = strrchr(c->bl[bi].path, '/');
            snprintf(rel, sizeof rel, "%s/%s", last ? last + 1 : c->bl[bi].path, fname);
            bl_dir(c, bi, dir, sizeof dir);
            snprintf(full, sizeof full, "%s/%s", dir, fname);
        } else {
            scopy(rel, sizeof rel, tmpname);
            snprintf(full, sizeof full, "%s/%s", c->music_dir, rel);
        }
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
    if (bi >= 0) base_id = c->bl[bi].id; else if (c->nbl > 0) base_id = c->bl[0].id;

    /* nowy identyfikator utworu */
    for (tries = 0; tries < 20; tries++) {
        int f;
        char q[160];
        long long cand = random63();
        snprintf(q, sizeof q, "SELECT 1 FROM item WHERE %s = %lld", c->ikey, cand);
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
    k_composer = ensure_entity(c, 4, "", 0, pid, res);   /* tylko istniejący pusty wiersz, jeśli jest */
    if (k_composer < 0) k_composer = 0;

    /* --- item --- */
    if (c->have_tpl_item) {
        row_copy(&it, &c->tpl_item);
        row_reset_foreign(&it, &c->item, NULL);
    }
    set_int(&it, &c->item, c->ikey, pid);
    set_int(&it, &c->item, "item_artist_pid", k_artist);
    set_int(&it, &c->item, "album_artist_pid", k_alb_artist);
    set_int(&it, &c->item, "album_pid", k_album);
    set_int(&it, &c->item, "genre_id", k_genre);
    set_int(&it, &c->item, "composer_pid", k_composer);
    set_int(&it, &c->item, "disc_number", tr->disc);
    set_int(&it, &c->item, "track_number", tr->track);
    set_int(&it, &c->item, "exclude_from_shuffle", 0);
    if (base_id) set_int(&it, &c->item, "base_location_id", base_id);
    /* dodatkowe pola starszych układów bazy (jeśli kiedyś się trafią) */
    set_int(&it, &c->item, "in_my_library", 1);
    set_int(&it, &c->item, "date_added", c->now_db);
    set_int(&it, &c->item, "date_downloaded", c->now_db);
    if (!c->have_tpl_item) set_int(&it, &c->item, "media_type", 8);
    set_order(c, &it, &c->item, "title", title);
    set_order(c, &it, &c->item, "item_artist", tr->artist);
    set_order(c, &it, &c->item, "album", tr->album);
    set_order(c, &it, &c->item, "album_artist", albumartist ? albumartist : tr->artist);
    set_order(c, &it, &c->item, "genre", tr->genre);
    set_order(c, &it, &c->item, "composer", "");
    if (!c->have_tpl_item) row_fill_required(&it, &c->item);
    for (tries = 0; tries < 4; tries++) {
        if (row_insert(c->db, &c->item, &it, c->r, errmsg, sizeof errmsg)) break;
        if (!fix_missing(c->db, errmsg, c->r)) { snprintf(res->error, sizeof res->error, "zapis do tabeli item nie powiódł się (%s)", errmsg); goto fail_sp; }
    }
    if (tries == 4) { scopy(res->error, sizeof res->error, "zapis do tabeli item nie powiódł się"); goto fail_sp; }
    rep(c->r, "  item: dodano (item_pid %lld)\n", pid);
    rev_add(c, pid, "item", c->ikey);

    /* --- item_extra --- */
    if (c->has_extra && col_index(&c->extra, "item_pid") >= 0) {
        int i_total, i_dur, i_sr;
        if (c->tpl_pid) have_ex = row_read(c->db, &c->extra, "item_pid = ?1", 1, c->tpl_pid, &ex);
        i_total = col_index(&c->extra, "total_time_ms");
        i_dur = col_index(&c->extra, "duration");
        i_sr = col_index(&c->extra, "sample_rate");
        /* jednostki czasu i częstotliwości odczytujemy ze wzoru, zanim go wyczyścimy */
        {
            long long tpl_total = (have_ex && i_total >= 0) ? val_ll(&ex.v[i_total]) : 0;
            long long tpl_dur = (have_ex && i_dur >= 0) ? val_ll(&ex.v[i_dur]) : 0;
            long long tpl_sr = (have_ex && i_sr >= 0) ? val_ll(&ex.v[i_sr]) : 0;
            int dur_real = have_ex && i_dur >= 0 && ex.v[i_dur].type == SQLITE_FLOAT;
            int total_real = have_ex && i_total >= 0 && ex.v[i_total].type == SQLITE_FLOAT;
            int dur_seconds = (tpl_total > 0 && tpl_dur > 0 && tpl_dur * 100 < tpl_total);
            if (have_ex) row_reset_foreign(&ex, &c->extra, NULL);
            set_int(&ex, &c->extra, "item_pid", pid);
            set_text(&ex, &c->extra, "title", title);
            set_null_col(&ex, &c->extra, "sort_title");
            set_int(&ex, &c->extra, "disc_count", tr->disc_total);
            set_int(&ex, &c->extra, "track_count", tr->track_total);
            if (total_real) set_real(&ex, &c->extra, "total_time_ms", (double)tr->duration_ms);
            else set_int(&ex, &c->extra, "total_time_ms", tr->duration_ms);
            if (i_dur >= 0) {
                if (dur_seconds) {
                    if (dur_real) set_real(&ex, &c->extra, "duration", (double)tr->duration_ms / 1000.0);
                    else set_int(&ex, &c->extra, "duration", (tr->duration_ms + 500) / 1000);
                } else {
                    if (dur_real) set_real(&ex, &c->extra, "duration", (double)tr->duration_ms);
                    else set_int(&ex, &c->extra, "duration", tr->duration_ms);
                }
            }
            if (i_sr >= 0 && tr->sample_rate > 0) {
                if (tpl_sr > 0 && tpl_sr <= 200000) {
                    if (ex.v[i_sr].type == SQLITE_FLOAT) set_real(&ex, &c->extra, "sample_rate", (double)tr->sample_rate);
                    else set_int(&ex, &c->extra, "sample_rate", tr->sample_rate);
                } else if (tpl_sr > 200000 && (tpl_sr & 0xFFFFLL) == 0) {
                    set_int(&ex, &c->extra, "sample_rate", ((long long)tr->sample_rate) << 16);
                } else if (!have_ex) {
                    set_int(&ex, &c->extra, "sample_rate", tr->sample_rate);
                }
            }
        }
        set_int(&ex, &c->extra, "year", tr->year);
        set_text(&ex, &c->extra, "location", c->fmode ? fname : rel);
        {
            struct stat sb;
            if (stat(full, &sb) == 0) set_int(&ex, &c->extra, "file_size", (long long)sb.st_size);
        }
        set_int(&ex, &c->extra, "date_created", c->now_db);
        set_int(&ex, &c->extra, "date_modified", c->now_db);
        set_int(&ex, &c->extra, "is_user_disabled", 0);
        set_int(&ex, &c->extra, "is_compilation", 0);
        set_int(&ex, &c->extra, "bpm", 0);
        set_int(&ex, &c->extra, "pending_delete", 0);
        if (!have_ex) { set_int(&ex, &c->extra, "media_kind", 1); row_fill_required(&ex, &c->extra); }
        if (!row_insert(c->db, &c->extra, &ex, c->r, errmsg, sizeof errmsg)) { snprintf(res->error, sizeof res->error, "zapis do item_extra nie powiódł się (%s)", errmsg); goto fail_sp; }
        rep(c->r, "  item_extra: dodano%s\n", have_ex ? "" : " [bez wzoru]");
    } else rep(c->r, "  (brak tabeli item_extra z kolumną item_pid)\n");

    /* --- item_search --- */
    if (!add_search(c, pid, title, tr->artist, tr->album, albumartist ? albumartist : tr->artist, res)) goto fail_sp;

    /* --- pozostałe tabele "per utwór" (item_stats, purgeable_list...) --- */
    for (k = 0; k < c->nptab; k++) {
        tbl_t t;
        row_t br;
        char e2[200];
        int have_b = 0, force = ieq(c->ptab[k], "item_stats");
        if (!tbl_load(c->db, c->ptab[k], &t)) continue;
        row_init(&br);
        if (c->tpl_pid) have_b = row_read(c->db, &t, "item_pid = ?1", 1, c->tpl_pid, &br);
        if (!have_b && !force) { row_free(&br); continue; }
        if (have_b) row_reset_foreign(&br, &t, NULL);
        set_int(&br, &t, "item_pid", pid);
        {
            int di = col_index(&t, "date_last_used");
            if (di >= 0 && have_b && val_ll(&br.v[di]) > 0) set_int(&br, &t, "date_last_used", c->now_db);
        }
        if (!have_b) row_fill_required(&br, &t);
        if (!row_insert(c->db, &t, &br, c->r, e2, sizeof e2)) {
            rep(c->r, "  uwaga: %s: %s (pomijam)\n", c->ptab[k], e2);
        } else rep(c->r, "  %s: dodano%s\n", c->ptab[k], have_b ? "" : " [bez wzoru]");
        row_free(&br);
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
    /* numery wersji nie mogą "wyprzedzać" tego, co jest w tabeli */
    {
        int f;
        long long m = query_int(c->db, "SELECT max(revision) FROM entity_revision", &f);
        if (c->have_rev && f) c->rev_max = m; else if (c->have_rev) c->rev_max = c->rev_old_max;
    }
fail:
    if (copied) remove(full);
    rep(c->r, "  BŁĄD: %s\n", res->error);
done:
    row_free(&it); row_free(&ex);
    return ok;
}

/* ------------------------------------------------------------------ opis bazy (diagnostyka) */

static void dump_val(rep_t *r, const char *name, const val_t *v)
{
    int k;
    switch (v->type) {
        case SQLITE_INTEGER: rep(r, " %s=%lld", name, v->i); break;
        case SQLITE_FLOAT: rep(r, " %s=%g", name, v->d); break;
        case SQLITE_TEXT: rep(r, " %s=\"%.48s\"", name, v->s ? v->s : ""); break;
        case SQLITE_BLOB:
            rep(r, " %s=<blob %d:", name, v->n);
            for (k = 0; k < v->n && k < 12; k++) rep(r, "%02x", v->b[k]);
            rep(r, ">");
            break;
        default: break;
    }
}

/* jeden wiersz tabeli: najpierw wartości, potem lista kolumn NULL */
static void dump_row(rep_t *r, sqlite3 *db, const char *table, const char *keycol, long long key)
{
    tbl_t t;
    row_t row;
    char w[120];
    int k, nulls = 0;
    if (!table_exists(db, table) || is_virtual_table(db, table) || !tbl_load(db, table, &t)) return;
    if (col_index(&t, keycol) < 0) return;
    snprintf(w, sizeof w, "%s = ?1", keycol);
    if (!row_read(db, &t, w, 1, key, &row)) { rep(r, " %s: (brak wiersza)\n", table); return; }
    rep(r, " %s:", table);
    for (k = 0; k < t.n; k++) {
        if (row.v[k].type == SQLITE_NULL) { nulls++; continue; }
        dump_val(r, t.c[k].name, &row.v[k]);
    }
    if (nulls) {
        rep(r, "  NULL:");
        for (k = 0; k < t.n; k++) if (row.v[k].type == SQLITE_NULL) rep(r, " %s", t.c[k].name);
    }
    rep(r, "\n");
    row_free(&row);
}

static void dump_query(rep_t *r, sqlite3 *db, const char *title, const char *sql)
{
    sqlite3_stmt *st = NULL;
    int any = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return;
    while (sqlite3_step(st) == SQLITE_ROW) {
        int cc = sqlite3_column_count(st), k;
        if (!any) rep(r, "\n%s:\n", title);
        any = 1;
        rep(r, " ");
        for (k = 0; k < cc; k++) {
            const char *s = (const char *)sqlite3_column_text(st, k);
            rep(r, " %s=%.60s", sqlite3_column_name(st, k), s ? s : "NULL");
        }
        rep(r, "\n");
    }
    sqlite3_finalize(st);
}

static void dump_item_sample(rep_t *r, sqlite3 *db, const char *ikey, long long pid)
{
    static const char *const per[] = { "item_extra", "item_stats", "item_search", "purgeable_list", NULL };
    static const struct { const char *tab, *alt, *ref; } ents[] = {
        { "item_artist", "artist", "item_artist_pid" }, { "album_artist", NULL, "album_artist_pid" },
        { "album", NULL, "album_pid" }, { "genre", NULL, "genre_id" }, { "composer", NULL, "composer_pid" }
    };
    tbl_t it;
    row_t row;
    int i, k;
    char w[120];
    rep(r, "\nPrzykładowy utwór %s=%lld:\n", ikey, pid);
    dump_row(r, db, "item", ikey, pid);
    for (i = 0; per[i]; i++) dump_row(r, db, per[i], "item_pid", pid);
    if (!tbl_load(db, "item", &it)) return;
    snprintf(w, sizeof w, "%s = ?1", ikey);
    if (row_read(db, &it, w, 1, pid, &row)) {
        for (k = 0; k < 5; k++) {
            int ci = col_index(&it, ents[k].ref);
            const char *tab = table_exists(db, ents[k].tab) ? ents[k].tab : ents[k].alt;
            tbl_t et;
            const char *keycol, *namecol;
            if (ci < 0 || !tab || row.v[ci].type != SQLITE_INTEGER || row.v[ci].i == 0) continue;
            if (!tbl_load(db, tab, &et) || !entity_names(&et, &keycol, &namecol)) continue;
            dump_row(r, db, tab, keycol, row.v[ci].i);
        }
        row_free(&row);
    }
    if (table_exists(db, "entity_revision")) {
        char q[400];
        snprintf(q, sizeof q, "SELECT * FROM entity_revision WHERE entity_pid IN (%lld, "
                 "(SELECT item_artist_pid FROM item WHERE %s=%lld), (SELECT album_pid FROM item WHERE %s=%lld), "
                 "(SELECT genre_id FROM item WHERE %s=%lld)) ORDER BY revision", pid, ikey, pid, ikey, pid, ikey, pid);
        dump_query(r, db, " entity_revision tego utworu", q);
    }
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
        for (k = 0; k < t.n; k++) {
            rep(r, " %s%s", t.c[k].name, t.c[k].pk ? "*" : "");
            if (t.c[k].type[0] && !icontains(t.c[k].type, "INT")) rep(r, ":%s", t.c[k].type);
        }
        rep(r, "\n");
    }
    /* wyzwalacze i indeksy */
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
    if (table_exists(db, "base_location")) dump_query(r, db, "base_location", "SELECT * FROM base_location ORDER BY path LIMIT 64");
    if (table_exists(db, "_MLDatabaseProperties")) dump_query(r, db, "_MLDatabaseProperties", "SELECT key, value FROM _MLDatabaseProperties");
    if (table_exists(db, "location_kind")) dump_query(r, db, "location_kind", "SELECT * FROM location_kind");
    if (table_exists(db, "sort_map")) {
        dump_query(r, db, "sort_map (pierwsze 25 wg order)", "SELECT name, name_order, name_section FROM sort_map ORDER BY name_order LIMIT 25");
        dump_query(r, db, "sort_map (ostatnie 5)", "SELECT name, name_order, name_section FROM sort_map ORDER BY name_order DESC LIMIT 5");
    }
    if (table_exists(db, "entity_revision")) {
        dump_query(r, db, "entity_revision: rodzaje", "SELECT class, revision_type, deleted, count(*) AS n, max(revision) AS max_rev FROM entity_revision GROUP BY class, revision_type, deleted");
        dump_query(r, db, "entity_revision: ostatnie 6", "SELECT * FROM entity_revision ORDER BY revision DESC LIMIT 6");
    }
    /* przykładowe utwory: pierwszy mp3, pierwszy nie-mp3, najnowszy */
    if (table_exists(db, "item") && table_exists(db, "item_extra")) {
        tbl_t it;
        char ikey[24];
        int f;
        long long a, b, cpid;
        if (tbl_load(db, "item", &it)) {
            if (col_index(&it, "item_pid") >= 0) scopy(ikey, sizeof ikey, "item_pid");
            else if (col_index(&it, "pid") >= 0) scopy(ikey, sizeof ikey, "pid");
            else ikey[0] = 0;
            if (ikey[0]) {
                char q[400];
                snprintf(q, sizeof q, "SELECT item.%s FROM item JOIN item_extra ON item_extra.item_pid = item.%s WHERE lower(item_extra.location) LIKE '%%.mp3' ORDER BY item.%s LIMIT 1", ikey, ikey, ikey);
                a = query_int(db, q, &f);
                if (f) dump_item_sample(r, db, ikey, a);
                snprintf(q, sizeof q, "SELECT item.%s FROM item JOIN item_extra ON item_extra.item_pid = item.%s WHERE lower(item_extra.location) NOT LIKE '%%.mp3' ORDER BY item.%s LIMIT 1", ikey, ikey, ikey);
                b = query_int(db, q, &f);
                if (f) dump_item_sample(r, db, ikey, b);
                if (col_index(&it, ikey) >= 0) {
                    snprintf(q, sizeof q, "SELECT item.%s FROM item JOIN item_extra ON item_extra.item_pid = item.%s ORDER BY item_extra.date_created DESC LIMIT 1", ikey, ikey);
                    cpid = query_int(db, q, &f);
                    if (f && (cpid != a || !f) && cpid != b) { rep(r, "\n(najnowszy wg date_created)"); dump_item_sample(r, db, ikey, cpid); }
                }
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

/* tabele z kolumną item_pid, które klonujemy ze wzorca (poza item, item_extra i item_search) */
static void scan_item_tables(ctx_t *c)
{
    sqlite3_stmt *st = NULL;
    char names[MAXT][48];
    int nt = 0, i;
    c->nptab = 0;
    if (sqlite3_prepare_v2(c->db, "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name", -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW && nt < MAXT) scopy(names[nt++], sizeof names[0], (const char *)sqlite3_column_text(st, 0));
        sqlite3_finalize(st);
    }
    for (i = 0; i < nt && c->nptab < 24; i++) {
        tbl_t t;
        if (ieq(names[i], "item") || ieq(names[i], "item_extra") || ieq(names[i], "item_search")) continue;
        if (is_virtual_table(c->db, names[i])) continue;
        if (!tbl_load(c->db, names[i], &t) || col_index(&t, "item_pid") < 0) continue;
        scopy(c->ptab[c->nptab++], sizeof c->ptab[0], names[i]);
    }
}

static void remove_new_files(ctx_t *c, mi_lib_result *res, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        if (res[i].ok && res[i].rel[0]) {
            char full[1100];
            snprintf(full, sizeof full, "%s/%s", c->music_dir, res[i].rel);
            remove(full);
        }
        res[i].ok = 0;
    }
}

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
    if (c->has_item) {
        if (col_index(&c->item, "item_pid") >= 0) scopy(c->ikey, sizeof c->ikey, "item_pid");
        else if (col_index(&c->item, "pid") >= 0) scopy(c->ikey, sizeof c->ikey, "pid");
    }
    if (!c->has_item || !c->ikey[0]) {
        rep(r, "W bazie nie ma tabeli item z kolumną item_pid - to nie wygląda na bibliotekę Muzyki z iOS 6.\n");
        rep(r, "\nOpis bazy (prześlij go autorowi):\n");
        diagnose_db(db, r);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL); sqlite3_close(db); free(c);
        return -1;
    }
    c->has_extra = tbl_load(db, "item_extra", &c->extra);
    tbl_load(db, "sort_map", &c->sortmap);
    scan_item_tables(c);
    {
        tbl_t er;
        if (table_exists(db, "entity_revision") && !is_virtual_table(db, "entity_revision") && tbl_load(db, "entity_revision", &er) &&
            col_index(&er, "revision") >= 0 && col_index(&er, "entity_pid") >= 0 && col_index(&er, "deleted") >= 0 &&
            col_index(&er, "class") >= 0 && col_index(&er, "revision_type") >= 0) {
            c->have_rev = 1;
            c->rev_old_max = query_int(db, "SELECT max(revision) FROM entity_revision", &f);
            c->rev_max = c->rev_old_max;
        }
    }
    rep(r, "Tabele: item (%d kolumn, klucz %s), item_extra (%d), sort_map (%d), entity_revision: %s\n",
        c->item.n, c->ikey, c->extra.n, c->sortmap.n, c->have_rev ? "jest" : "brak");
    rep(r, "Tabele per utwór:");
    for (i = 0; i < c->nptab; i++) rep(r, " %s", c->ptab[i]);
    rep(r, "\n");

    prepare_music_dir(c, db_path);
    rep(r, "Folder Muzyki: %s (%s, wierszy base_location: %d)\n", c->music_dir, c->fmode ? "osobny wiersz na folder Fxx" : "jeden wiersz", c->nbl);
    if (!c->fmode && !dir_exists(c->music_dir) && !mkdir_p(c->music_dir)) {
        rep(r, "Nie ma folderu Muzyki i nie mogę go utworzyć.\n");
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL); sqlite3_close(db); free(c);
        return -1;
    }

    for (i = 0; i < n; i++) {
        const mi_lib_track *tr = &tracks[i];
        const char *ext = mi_ext_for_format(tr->format);
        rep(r, "\n[%d/%d] %s - %s\n", i + 1, n, tr->artist && tr->artist[0] ? tr->artist : "?", tr->title && tr->title[0] ? tr->title : "?");
        /* wzór dobieramy do rozszerzenia: przy zmianie formatu wczytujemy nowy */
        if (!c->tpl_loaded || strcmp(c->tpl_ext, ext) != 0) {
            if (c->have_tpl_item) row_free(&c->tpl_item);
            c->have_tpl_item = 0;
            load_template(c, ext);
            if (c->tpl_pid) {
                char w[60];
                snprintf(w, sizeof w, "%s = ?1", c->ikey);
                c->have_tpl_item = row_read(db, &c->item, w, 1, c->tpl_pid, &c->tpl_item);
                rep(r, "Wzór (.%s): utwór %lld\n", ext, c->tpl_pid);
            } else rep(r, "Biblioteka jest pusta - zapisuję bez wzoru (mniej pewne)\n");
            detect_dates(c, now);
            rep(r, "Daty w bazie: %s\n", c->unix_dates ? "od 1970" : "od 2001");
            c->tpl_loaded = 1;
            scopy(c->tpl_ext, sizeof c->tpl_ext, ext);
        }
        if (import_one(c, tr, &res[i])) good++;
    }

    /* licznik wersji w _MLDatabaseProperties (tylko gdy dokładnie zgadza się ze starym maksimum z entity_revision) */
    if (good > 0 && c->have_rev && c->rev_max > c->rev_old_max && table_exists(db, "_MLDatabaseProperties")) {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db, "UPDATE _MLDatabaseProperties SET value = ?1 WHERE key LIKE '%revision%' AND CAST(value AS INTEGER) = ?2 AND CAST(value AS INTEGER) > 0", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(st, 1, c->rev_max);
            sqlite3_bind_int64(st, 2, c->rev_old_max);
            if (sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(db) > 0) rep(r, "\nLicznik wersji bazy: %lld -> %lld\n", c->rev_old_max, c->rev_max);
            sqlite3_finalize(st);
        }
    }

    /* kontrola po zmianach */
    if (!run_quick_check(db, r, "Kontrola po zmianach")) {
        rep(r, "\nKontrola spójności po zmianach NIE przeszła - wycofuję wszystko.\n");
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        remove_new_files(c, res, n);
        for (i = 0; i < n; i++) scopy(res[i].error, sizeof res[i].error, "baza po zmianach nie przeszła kontroli spójności (wycofano)");
        if (c->have_tpl_item) row_free(&c->tpl_item);
        sqlite3_close(db); free(c);
        return -1;
    }
    if (good > 0) {
        if (sqlite3_exec(db, "COMMIT", NULL, NULL, &err) != SQLITE_OK) {
            rep(r, "\nNie udało się zatwierdzić zmian: %s\n", err ? err : "?");
            sqlite3_free(err);
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            remove_new_files(c, res, n);
            for (i = 0; i < n; i++) scopy(res[i].error, sizeof res[i].error, "nie udało się zatwierdzić zmian w bazie");
            if (c->have_tpl_item) row_free(&c->tpl_item);
            sqlite3_close(db); free(c);
            return -1;
        }
        rep(r, "\nZatwierdzono zmiany: %d z %d utworów.\n", good, n);
    } else {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        rep(r, "\nNic nie zapisano.\n");
    }
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
    char q[900], qi[110], ikey[24];
    ctx_t *c;
    tbl_t it;
    char *err = NULL;

    rp.buf = report; rp.cap = cap; rp.len = 0;
    if (report && cap > 0) report[0] = 0;
    if (!open_db(db_path, &db, r)) return 0;
    c = (ctx_t *)calloc(1, sizeof *c);
    if (!c) { sqlite3_close(db); return 0; }
    c->db = db; c->r = r;
    if (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, &err) != SQLITE_OK) {
        rep(r, "Baza jest zajęta (%s). Zamknij Muzykę.\n", err ? err : "?");
        sqlite3_free(err);
        sqlite3_close(db); free(c);
        return 0;
    }
    ikey[0] = 0;
    if (tbl_load(db, "item", &it)) {
        if (col_index(&it, "item_pid") >= 0) scopy(ikey, sizeof ikey, "item_pid");
        else if (col_index(&it, "pid") >= 0) scopy(ikey, sizeof ikey, "pid");
    }
    if (!ikey[0]) {
        rep(r, "W bazie nie ma tabeli item z kluczem item_pid.\n");
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL); sqlite3_close(db); free(c);
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
        if (ieq(names[i], "item")) continue;
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
    snprintf(q, sizeof q, "DELETE FROM item WHERE %s = %lld", ikey, pid);
    if (sqlite3_exec(db, q, NULL, NULL, &err) != SQLITE_OK) {
        rep(r, "Nie udało się usunąć wiersza z item: %s\n", err ? err : "?");
        sqlite3_free(err);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db); free(c);
        return 0;
    }
    if (table_exists(db, "entity_revision")) {
        snprintf(q, sizeof q, "DELETE FROM entity_revision WHERE entity_pid = %lld", pid);
        sqlite3_exec(db, q, NULL, NULL, NULL);
    }
    /* wykonawcy / albumy / gatunki, które dodaliśmy i nikt już ich nie używa */
    for (i = 0; i < nent; i++) {
        int f;
        char qt[110], qk[110];
        long long cnt;
        if (!ents[i].table[0] || !ents[i].keycol[0] || !ents[i].refcol[0]) continue;
        if (col_index(&it, ents[i].refcol) < 0) continue;
        quote_ident(qi, sizeof qi, ents[i].refcol);
        snprintf(q, sizeof q, "SELECT count(*) FROM item WHERE %s = %lld", qi, ents[i].key);
        cnt = query_int(db, q, &f);
        if (f && cnt == 0) {
            quote_ident(qt, sizeof qt, ents[i].table);
            quote_ident(qk, sizeof qk, ents[i].keycol);
            snprintf(q, sizeof q, "DELETE FROM %s WHERE %s = %lld", qt, qk, ents[i].key);
            sqlite3_exec(db, q, NULL, NULL, NULL);
            if (table_exists(db, "entity_revision")) {
                snprintf(q, sizeof q, "DELETE FROM entity_revision WHERE entity_pid = %lld", ents[i].key);
                sqlite3_exec(db, q, NULL, NULL, NULL);
            }
            rep(r, "Usunięto z %s niepotrzebny wiersz %lld\n", ents[i].table, ents[i].key);
        }
    }
    /* wiersze, które wskazywały na usunięty utwór jako "reprezentatywny": przepinamy na inny utwór */
    for (i = 0; i < 5; i++) {
        const char *table = resolve_table(db, &kEnt[i]);
        tbl_t te;
        const char *keycol, *namecol;
        char qt[110], qk[110], qr[110];
        if (!table || !tbl_load(db, table, &te) || !entity_names(&te, &keycol, &namecol)) continue;
        if (col_index(&te, "representative_item_pid") < 0 || col_index(&it, kEnt[i].refcol) < 0) continue;
        quote_ident(qt, sizeof qt, table);
        quote_ident(qk, sizeof qk, keycol);
        quote_ident(qr, sizeof qr, kEnt[i].refcol);
        snprintf(q, sizeof q,
                 "UPDATE %s SET representative_item_pid = COALESCE((SELECT item.%s FROM item WHERE item.%s = %s.%s LIMIT 1), 0) "
                 "WHERE representative_item_pid = %lld", qt, ikey, qr, qt, qk, pid);
        sqlite3_exec(db, q, NULL, NULL, NULL);
    }
    if (sqlite3_exec(db, "COMMIT", NULL, NULL, &err) != SQLITE_OK) {
        rep(r, "Nie udało się zatwierdzić: %s\n", err ? err : "?");
        sqlite3_free(err);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        sqlite3_close(db); free(c);
        return 0;
    }
    ok = 1;
    if (rel && rel[0]) {
        prepare_music_dir(c, db_path);
        {
            char full[1100];
            snprintf(full, sizeof full, "%s/%s", c->music_dir, rel);
            if (file_exists(full)) { remove(full); rep(r, "Usunięto plik %s\n", rel); }
        }
    }
    rep(r, "Usunięto utwór %lld z biblioteki.\n", pid);
    sqlite3_close(db);
    free(c);
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
