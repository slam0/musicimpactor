#include "mi_core.h"

/* ---------- drobne funkcje własne ---------- */

static long slen(const char *s) { long n = 0; while (s[n]) n++; return n; }
static int is_space(unsigned char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static int is_digit(int c) { return c >= '0' && c <= '9'; }
static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static void scopy(char *dst, int n, const char *src)
{
    int i = 0;
    if (n <= 0) return;
    while (src[i] && i < n - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static int ieq(const char *a, const char *b)
{
    while (*a && *b) { if (lower((unsigned char)*a) != lower((unsigned char)*b)) return 0; a++; b++; }
    return *a == *b;
}

static unsigned int be32(const unsigned char *p) { return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3]; }
static unsigned int be24(const unsigned char *p) { return ((unsigned int)p[0] << 16) | ((unsigned int)p[1] << 8) | p[2]; }
static unsigned int be16(const unsigned char *p) { return ((unsigned int)p[0] << 8) | p[1]; }
static unsigned int le32(const unsigned char *p) { return ((unsigned int)p[3] << 24) | ((unsigned int)p[2] << 16) | ((unsigned int)p[1] << 8) | p[0]; }
static unsigned int le16(const unsigned char *p) { return ((unsigned int)p[1] << 8) | p[0]; }
static unsigned int syncsafe(const unsigned char *p) { return ((unsigned int)(p[0] & 0x7F) << 21) | ((unsigned int)(p[1] & 0x7F) << 14) | ((unsigned int)(p[2] & 0x7F) << 7) | (p[3] & 0x7F); }

static int same4(const unsigned char *p, const char *s) { return p[0] == (unsigned char)s[0] && p[1] == (unsigned char)s[1] && p[2] == (unsigned char)s[2] && p[3] == (unsigned char)s[3]; }

/* ---------- UTF-8 ---------- */

typedef struct { char *buf; int cap; int len; } outbuf;

static void put_cp(outbuf *o, unsigned int cp)
{
    if (cp < 0x80) { if (o->len + 1 < o->cap) o->buf[o->len++] = (char)cp; }
    else if (cp < 0x800) { if (o->len + 2 < o->cap) { o->buf[o->len++] = (char)(0xC0 | (cp >> 6)); o->buf[o->len++] = (char)(0x80 | (cp & 0x3F)); } }
    else if (cp < 0x10000) { if (o->len + 3 < o->cap) { o->buf[o->len++] = (char)(0xE0 | (cp >> 12)); o->buf[o->len++] = (char)(0x80 | ((cp >> 6) & 0x3F)); o->buf[o->len++] = (char)(0x80 | (cp & 0x3F)); } }
    else { if (o->len + 4 < o->cap) { o->buf[o->len++] = (char)(0xF0 | (cp >> 18)); o->buf[o->len++] = (char)(0x80 | ((cp >> 12) & 0x3F)); o->buf[o->len++] = (char)(0x80 | ((cp >> 6) & 0x3F)); o->buf[o->len++] = (char)(0x80 | (cp & 0x3F)); } }
}

/* Windows-1250 (polskie i środkowoeuropejskie litery) - bajty 0x80..0xFF */
static const unsigned short kCp1250[128] = {
    0x20AC, 0xFFFD, 0x201A, 0xFFFD, 0x201E, 0x2026, 0x2020, 0x2021,
    0xFFFD, 0x2030, 0x0160, 0x2039, 0x015A, 0x0164, 0x017D, 0x0179,
    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0xFFFD, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A,
    0x00A0, 0x02C7, 0x02D8, 0x0141, 0x00A4, 0x0104, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x015E, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x017B,
    0x00B0, 0x00B1, 0x02DB, 0x0142, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
    0x00B8, 0x0105, 0x015F, 0x00BB, 0x013D, 0x02DD, 0x013E, 0x017C,
    0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
    0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
    0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
    0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
    0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
    0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
    0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};

static int valid_utf8(const unsigned char *p, long n)
{
    long i = 0;
    int multi = 0;
    while (i < n) {
        unsigned char c = p[i];
        int extra = 0;
        if (c < 0x80) { i++; continue; }
        if (c >= 0xC2 && c <= 0xDF) extra = 1;
        else if (c >= 0xE0 && c <= 0xEF) extra = 2;
        else if (c >= 0xF0 && c <= 0xF4) extra = 3;
        else return 0;
        if (i + extra >= n) return 0;
        { int k; for (k = 1; k <= extra; k++) if ((p[i + k] & 0xC0) != 0x80) return 0; }
        i += extra + 1;
        multi = 1;
    }
    return multi ? 2 : 1;      /* 2 = są wielobajtowe znaki, 1 = sam ASCII */
}

/* Bajtowe kodowanie "Latin-1" w tagach bywa w rzeczywistości Windows-1250 (polskie pliki). Rozpoznajemy po literach, których w Latin-1 prawie nie ma. */
static int looks_cp1250(const unsigned char *p, long n)
{
    long i;
    for (i = 0; i < n; i++) {
        unsigned char c = p[i];
        if (c >= 0x80 && c <= 0x9F) return 1;
        if (c == 0xB9 || c == 0xB3 || c == 0xBF || c == 0xA5 || c == 0xA3 || c == 0xAF) return 1;
    }
    return 0;
}

static void bytes_to_utf8(const unsigned char *p, long n, outbuf *o)
{
    long i;
    int v = valid_utf8(p, n);
    if (v == 2) { for (i = 0; i < n; i++) if (o->len + 1 < o->cap) o->buf[o->len++] = (char)p[i]; return; }
    {
        int cp = looks_cp1250(p, n);
        for (i = 0; i < n; i++) {
            unsigned char c = p[i];
            if (c < 0x80) put_cp(o, c);
            else if (cp) { unsigned int u = kCp1250[c - 0x80]; put_cp(o, u == 0xFFFD ? '?' : u); }
            else put_cp(o, c);
        }
    }
}

static void utf16_to_utf8(const unsigned char *p, long n, int big, outbuf *o)
{
    long i = 0;
    while (i + 1 < n) {
        unsigned int u = big ? be16(p + i) : le16(p + i);
        i += 2;
        if (u == 0) break;
        if (u >= 0xD800 && u < 0xDC00 && i + 1 < n) {
            unsigned int u2 = big ? be16(p + i) : le16(p + i);
            if (u2 >= 0xDC00 && u2 < 0xE000) { i += 2; u = 0x10000 + (((u - 0xD800) << 10) | (u2 - 0xDC00)); }
        }
        put_cp(o, u);
    }
}

/* tekst ramki ID3: pierwszy bajt to kodowanie */
static void id3_text(const unsigned char *p, long n, char *dst, int dstn)
{
    outbuf o;
    int enc;
    o.buf = dst; o.cap = dstn; o.len = 0;
    if (dstn <= 0) return;
    dst[0] = 0;
    if (n < 1) return;
    enc = p[0]; p++; n--;
    if (enc == 1 || enc == 2) {
        int big = (enc == 2);
        if (n >= 2 && p[0] == 0xFF && p[1] == 0xFE) { big = 0; p += 2; n -= 2; }
        else if (n >= 2 && p[0] == 0xFE && p[1] == 0xFF) { big = 1; p += 2; n -= 2; }
        utf16_to_utf8(p, n, big, &o);
    } else {
        long m = 0;
        while (m < n && p[m]) m++;           /* do pierwszego zera (kolejne wartości pomijamy) */
        if (enc == 3) { long i; for (i = 0; i < m; i++) if (o.len + 1 < o.cap) o.buf[o.len++] = (char)p[i]; }
        else bytes_to_utf8(p, m, &o);
    }
    dst[o.len < dstn ? o.len : dstn - 1] = 0;
    {   /* spacje z brzegów */
        char tmp[512];
        mi_clean_text(dst, tmp, (int)sizeof tmp, dstn - 1);
        scopy(dst, dstn, tmp);
    }
}

/* ---------- czyszczenie tekstu ---------- */

int mi_clean_text(const char *in, char *out, int n, int max_bytes)
{
    int len = 0, pending_space = 0;
    const unsigned char *p = (const unsigned char *)in;
    if (n <= 0) return 0;
    if (max_bytes > n - 1) max_bytes = n - 1;
    while (p && *p) {
        if (is_space(*p) || *p < 0x20) {
            if (len > 0) pending_space = 1;
            p++;
            continue;
        }
        {
            int clen = 1, i, need;
            if (*p >= 0xF0) clen = 4;
            else if (*p >= 0xE0) clen = 3;
            else if (*p >= 0xC0) clen = 2;
            for (i = 1; i < clen; i++) if (!p[i]) { clen = i; break; }
            need = clen + (pending_space ? 1 : 0);
            if (len + need > max_bytes) break;
            if (pending_space) { out[len++] = ' '; pending_space = 0; }
            for (i = 0; i < clen; i++) out[len++] = (char)p[i];
            p += clen;
        }
    }
    out[len] = 0;
    return len;
}

/* ---------- format ---------- */

static const int kBr1[3][15] = {
    { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },     /* MPEG1 warstwa I */
    { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 },        /* MPEG1 warstwa II */
    { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 }          /* MPEG1 warstwa III */
};
static const int kBr2[2][15] = {
    { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 },        /* MPEG2/2.5 warstwa I */
    { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 }              /* MPEG2/2.5 warstwa II i III */
};
static const int kSr[3][3] = { { 44100, 48000, 32000 }, { 22050, 24000, 16000 }, { 11025, 12000, 8000 } };

typedef struct { int ver, layer, br, sr, pad, mono, flen, spf; } mp3hdr;

/* ver: 0 = MPEG1, 1 = MPEG2, 2 = MPEG2.5; layer: 1..3 */
static int parse_mp3_header(const unsigned char *p, mp3hdr *h)
{
    int vb, lb, bi, si;
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return 0;
    vb = (p[1] >> 3) & 3;
    lb = (p[1] >> 1) & 3;
    if (vb == 1 || lb == 0) return 0;
    bi = (p[2] >> 4) & 15;
    si = (p[2] >> 2) & 3;
    if (bi == 0 || bi == 15 || si == 3) return 0;
    h->ver = vb == 3 ? 0 : (vb == 2 ? 1 : 2);
    h->layer = 4 - lb;
    h->sr = kSr[h->ver][si];
    if (h->ver == 0) h->br = kBr1[h->layer - 1][bi];
    else h->br = kBr2[h->layer == 1 ? 0 : 1][bi];
    h->pad = (p[2] >> 1) & 1;
    h->mono = ((p[3] >> 6) & 3) == 3;
    if (h->layer == 1) { h->spf = 384; h->flen = ((int)(12.0 * h->br * 1000.0 / h->sr) + h->pad) * 4; }
    else if (h->layer == 2) { h->spf = 1152; h->flen = (int)(144.0 * h->br * 1000.0 / h->sr) + h->pad; }
    else if (h->ver == 0) { h->spf = 1152; h->flen = (int)(144.0 * h->br * 1000.0 / h->sr) + h->pad; }
    else { h->spf = 576; h->flen = (int)(72.0 * h->br * 1000.0 / h->sr) + h->pad; }
    return h->flen > 4;
}

static long id3v2_size(const unsigned char *b, long n)
{
    if (n >= 10 && b[0] == 'I' && b[1] == 'D' && b[2] == '3' && b[3] < 0xFF && b[4] < 0xFF) {
        long sz = (long)syncsafe(b + 6) + 10;
        if (b[5] & 0x10) sz += 10;      /* stopka */
        return sz;
    }
    return 0;
}

/* szuka pierwszej poprawnej ramki MP3 (z potwierdzeniem następną ramką) od pozycji start; zwraca pozycję albo -1 */
static long find_mp3_frame(const unsigned char *b, long n, long start, mp3hdr *h)
{
    long i, limit = start + 65536;
    if (limit > n - 4) limit = n - 4;
    for (i = start; i <= limit; i++) {
        if (b[i] == 0xFF && (b[i + 1] & 0xE0) == 0xE0 && parse_mp3_header(b + i, h)) {
            long nx = i + h->flen;
            mp3hdr h2;
            if (nx + 4 <= n) {
                if (parse_mp3_header(b + nx, &h2) && h2.sr == h->sr && h2.layer == h->layer) return i;
            } else {
                return i;           /* plik kończy się na pierwszej ramce */
            }
        }
    }
    return -1;
}

int mi_detect(const unsigned char *b, long n)
{
    long off;
    mp3hdr h;
    if (n < 4) return MI_FMT_UNKNOWN;
    if (n >= 12 && same4(b + 4, "ftyp")) return MI_FMT_M4A;
    if (n >= 12 && same4(b, "RIFF") && same4(b + 8, "WAVE")) return MI_FMT_WAV;
    if (same4(b, "fLaC")) return MI_FMT_FLAC;
    if (same4(b, "OggS")) return MI_FMT_OGG;
    if (n >= 12 && same4(b, "FORM") && (same4(b + 8, "AIFF") || same4(b + 8, "AIFC"))) return MI_FMT_AIFF;
    if (b[0] == 0x30 && b[1] == 0x26 && b[2] == 0xB2 && b[3] == 0x75) return MI_FMT_WMA;
    if (b[0] == '<') {
        long i; int lt = 0;
        for (i = 0; i < n && i < 512; i++) if (b[i] == '<') lt++;
        if (lt > 0 && (b[1] == '!' || b[1] == 'h' || b[1] == 'H' || b[1] == '?')) return MI_FMT_HTML;
    }
    off = id3v2_size(b, n);
    if (off > 0 && off >= n) return MI_FMT_MP3;           /* sam początek z tagiem */
    if (find_mp3_frame(b, n, off, &h) >= 0) return MI_FMT_MP3;
    if (off > 0) return MI_FMT_MP3;                          /* ID3, ale w próbce nie widać ramki */
    return MI_FMT_UNKNOWN;
}

int mi_format_supported(int fmt) { return fmt == MI_FMT_MP3 || fmt == MI_FMT_M4A || fmt == MI_FMT_WAV; }

const char *mi_ext_for_format(int fmt)
{
    switch (fmt) {
        case MI_FMT_MP3: return "mp3";
        case MI_FMT_M4A: return "m4a";
        case MI_FMT_WAV: return "wav";
        case MI_FMT_FLAC: return "flac";
        case MI_FMT_OGG: return "ogg";
        case MI_FMT_AIFF: return "aiff";
        case MI_FMT_WMA: return "wma";
        default: return "";
    }
}

/* ---------- tagi ---------- */

void mi_tags_clear(mi_tags *t)
{
    int i;
    unsigned char *p = (unsigned char *)t;
    for (i = 0; i < (int)sizeof(mi_tags); i++) p[i] = 0;
}

static int count_tags(const mi_tags *t)
{
    int c = 0;
    if (t->title[0]) c++;
    if (t->artist[0]) c++;
    if (t->album[0]) c++;
    if (t->albumartist[0]) c++;
    if (t->genre[0]) c++;
    if (t->year) c++;
    if (t->track) c++;
    if (t->disc) c++;
    return c;
}

static const char *const kGenres[80] = { "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock" };

const char *mi_genre_name(int idx) { return (idx >= 0 && idx < 80) ? kGenres[idx] : (const char *)0; }

/* "3/12" -> 3 i 12 */
static void parse_pair(const char *s, int *a, int *b)
{
    int v = 0, seen = 0;
    while (*s && !is_digit((unsigned char)*s)) s++;
    while (is_digit((unsigned char)*s)) { if (v < 100000) v = v * 10 + (*s - '0'); s++; seen = 1; }
    if (seen) *a = v;
    if (*s == '/') {
        v = 0; seen = 0; s++;
        while (is_digit((unsigned char)*s)) { if (v < 100000) v = v * 10 + (*s - '0'); s++; seen = 1; }
        if (seen) *b = v;
    }
}

static int parse_year(const char *s)
{
    int i, v = 0;
    for (i = 0; i < 4; i++) { if (!is_digit((unsigned char)s[i])) return 0; v = v * 10 + (s[i] - '0'); }
    return (v >= 1000 && v <= 2999) ? v : 0;
}

/* "(17)Rock", "(17)" albo "17" albo "Rock" */
static void fix_genre(char *g, int n)
{
    char tmp[128];
    const char *p = g;
    if (p[0] == '(') {
        int v = 0, k = 1;
        while (is_digit((unsigned char)p[k])) { v = v * 10 + (p[k] - '0'); k++; }
        if (p[k] == ')') {
            if (p[k + 1]) { scopy(tmp, (int)sizeof tmp, p + k + 1); scopy(g, n, tmp); return; }
            if (mi_genre_name(v)) { scopy(g, n, mi_genre_name(v)); return; }
        }
    } else if (is_digit((unsigned char)p[0])) {
        int v = 0, k = 0;
        while (is_digit((unsigned char)p[k])) { v = v * 10 + (p[k] - '0'); k++; }
        if (!p[k] && mi_genre_name(v)) scopy(g, n, mi_genre_name(v));
    }
}

static void set_frame(mi_tags *t, const char *id, const unsigned char *d, long len)
{
    char tmp[256];
    if (len < 1) return;
    if (ieq(id, "TIT2") || ieq(id, "TT2")) id3_text(d, len, t->title, (int)sizeof t->title);
    else if (ieq(id, "TPE1") || ieq(id, "TP1")) id3_text(d, len, t->artist, (int)sizeof t->artist);
    else if (ieq(id, "TALB") || ieq(id, "TAL")) id3_text(d, len, t->album, (int)sizeof t->album);
    else if (ieq(id, "TPE2") || ieq(id, "TP2")) id3_text(d, len, t->albumartist, (int)sizeof t->albumartist);
    else if (ieq(id, "TCON") || ieq(id, "TCO")) { id3_text(d, len, t->genre, (int)sizeof t->genre); fix_genre(t->genre, (int)sizeof t->genre); }
    else if (ieq(id, "TRCK") || ieq(id, "TRK")) { id3_text(d, len, tmp, (int)sizeof tmp); parse_pair(tmp, &t->track, &t->track_total); }
    else if (ieq(id, "TPOS") || ieq(id, "TPA")) { id3_text(d, len, tmp, (int)sizeof tmp); parse_pair(tmp, &t->disc, &t->disc_total); }
    else if (ieq(id, "TYER") || ieq(id, "TYE") || ieq(id, "TDRC") || ieq(id, "TDRL")) {
        int y; id3_text(d, len, tmp, (int)sizeof tmp); y = parse_year(tmp);
        if (y && (!t->year || ieq(id, "TDRC") || ieq(id, "TYER") || ieq(id, "TYE"))) t->year = y;
    }
}

static void read_id3v2(const unsigned char *b, long n, mi_tags *t)
{
    int ver = b[3];
    long size = (long)syncsafe(b + 6);
    long pos = 10, end;
    if (ver < 2 || ver > 4) return;
    end = 10 + size;
    if (end > n) end = n;
    if (ver >= 3 && (b[5] & 0x40) && pos + 4 <= end) {          /* nagłówek rozszerzony */
        long ext = ver == 4 ? (long)syncsafe(b + pos) : (long)be32(b + pos) + 4;
        if (ext > 0 && pos + ext <= end) pos += ext;
    }
    while (pos < end) {
        char id[5];
        long fsz, hdr;
        if (ver == 2) {
            if (pos + 6 > end || b[pos] == 0) break;
            id[0] = (char)b[pos]; id[1] = (char)b[pos + 1]; id[2] = (char)b[pos + 2]; id[3] = 0; id[4] = 0;
            fsz = (long)be24(b + pos + 3); hdr = 6;
        } else {
            if (pos + 10 > end || b[pos] == 0) break;
            id[0] = (char)b[pos]; id[1] = (char)b[pos + 1]; id[2] = (char)b[pos + 2]; id[3] = (char)b[pos + 3]; id[4] = 0;
            fsz = ver == 4 ? (long)syncsafe(b + pos + 4) : (long)be32(b + pos + 4);
            hdr = 10;
        }
        if (fsz < 0 || pos + hdr + fsz > end) break;
        if (id[0] == 'T') set_frame(t, id, b + pos + hdr, fsz);
        pos += hdr + fsz;
    }
}

static void v1_field(const unsigned char *p, int n, char *dst, int dstn)
{
    outbuf o;
    int m = 0;
    while (m < n && p[m]) m++;
    o.buf = dst; o.cap = dstn; o.len = 0;
    bytes_to_utf8(p, m, &o);
    dst[o.len] = 0;
    { char tmp[256]; mi_clean_text(dst, tmp, (int)sizeof tmp, dstn - 1); scopy(dst, dstn, tmp); }
}

static void read_id3v1(const unsigned char *b, long n, mi_tags *t)
{
    const unsigned char *p;
    char tmp[8];
    if (n < 128) return;
    p = b + n - 128;
    if (!(p[0] == 'T' && p[1] == 'A' && p[2] == 'G')) return;
    if (!t->title[0]) v1_field(p + 3, 30, t->title, (int)sizeof t->title);
    if (!t->artist[0]) v1_field(p + 33, 30, t->artist, (int)sizeof t->artist);
    if (!t->album[0]) v1_field(p + 63, 30, t->album, (int)sizeof t->album);
    if (!t->year) { int i; for (i = 0; i < 4; i++) tmp[i] = (char)p[93 + i]; tmp[4] = 0; t->year = parse_year(tmp); }
    if (!t->track && p[125] == 0 && p[126] != 0) t->track = p[126];
    if (!t->genre[0] && p[127] < 80) scopy(t->genre, (int)sizeof t->genre, kGenres[p[127]]);
}

/* --- MP4 --- */

typedef struct { long start, end; } range;

/* szuka atomu o podanym typie w [s,e); zwraca 1 i zakres danych (po nagłówku) */
static int mp4_find(const unsigned char *b, long s, long e, const unsigned char *type, range *out)
{
    long pos = s;
    while (pos + 8 <= e) {
        unsigned int sz = be32(b + pos);
        long size = (long)sz, hdr = 8;
        if (sz == 1) {
            if (pos + 16 > e || be32(b + pos + 8) != 0) return 0;
            size = (long)be32(b + pos + 12); hdr = 16;
        } else if (sz == 0) {
            size = e - pos;
        }
        if (size < hdr || pos + size > e) {
            if (pos + size > e && size >= hdr && b[pos + 4] == type[0] && b[pos + 5] == type[1] && b[pos + 6] == type[2] && b[pos + 7] == type[3]) { out->start = pos + hdr; out->end = e; return 1; }
            return 0;
        }
        if (b[pos + 4] == type[0] && b[pos + 5] == type[1] && b[pos + 6] == type[2] && b[pos + 7] == type[3]) { out->start = pos + hdr; out->end = pos + size; return 1; }
        pos += size;
    }
    return 0;
}

static int mp4_data_text(const unsigned char *b, range item, char *dst, int dstn)
{
    range d;
    long len;
    outbuf o;
    if (!mp4_find(b, item.start, item.end, (const unsigned char *)"data", &d)) return 0;
    if (d.end - d.start < 8) return 0;
    len = d.end - d.start - 8;
    o.buf = dst; o.cap = dstn; o.len = 0;
    { long i; for (i = 0; i < len; i++) if (o.len + 1 < o.cap) o.buf[o.len++] = (char)b[d.start + 8 + i]; }
    dst[o.len] = 0;
    { char tmp[512]; mi_clean_text(dst, tmp, (int)sizeof tmp, dstn - 1); scopy(dst, dstn, tmp); }
    return 1;
}

static void read_mp4_tags(const unsigned char *b, long n, mi_tags *t)
{
    range moov, udta, meta, ilst, it;
    long pos;
    if (!mp4_find(b, 0, n, (const unsigned char *)"moov", &moov)) return;
    if (!mp4_find(b, moov.start, moov.end, (const unsigned char *)"udta", &udta)) return;
    if (!mp4_find(b, udta.start, udta.end, (const unsigned char *)"meta", &meta)) return;
    /* 'meta' to zwykle pełny atom (4 bajty wersji i flag przed dziećmi), w starszych plikach QuickTime bez nich */
    if (!mp4_find(b, meta.start + 4, meta.end, (const unsigned char *)"ilst", &ilst)) {
        if (!mp4_find(b, meta.start, meta.end, (const unsigned char *)"ilst", &ilst)) return;
    }
    pos = ilst.start;
    while (pos + 8 <= ilst.end) {
        unsigned int sz = be32(b + pos);
        const unsigned char *ty = b + pos + 4;
        if (sz < 8 || pos + (long)sz > ilst.end) break;
        it.start = pos + 8; it.end = pos + (long)sz;
        if (ty[0] == 0xA9 && ty[1] == 'n' && ty[2] == 'a' && ty[3] == 'm') mp4_data_text(b, it, t->title, (int)sizeof t->title);
        else if (ty[0] == 0xA9 && ty[1] == 'A' && ty[2] == 'R' && ty[3] == 'T') mp4_data_text(b, it, t->artist, (int)sizeof t->artist);
        else if (ty[0] == 0xA9 && ty[1] == 'a' && ty[2] == 'l' && ty[3] == 'b') mp4_data_text(b, it, t->album, (int)sizeof t->album);
        else if (same4(ty, "aART")) mp4_data_text(b, it, t->albumartist, (int)sizeof t->albumartist);
        else if (ty[0] == 0xA9 && ty[1] == 'g' && ty[2] == 'e' && ty[3] == 'n') mp4_data_text(b, it, t->genre, (int)sizeof t->genre);
        else if (ty[0] == 0xA9 && ty[1] == 'd' && ty[2] == 'a' && ty[3] == 'y') { char tmp[64]; if (mp4_data_text(b, it, tmp, (int)sizeof tmp)) t->year = parse_year(tmp); }
        else if (same4(ty, "trkn") || same4(ty, "disk")) {
            range d;
            if (mp4_find(b, it.start, it.end, (const unsigned char *)"data", &d) && d.end - d.start >= 8 + 6) {
                const unsigned char *q = b + d.start + 8;
                int idx = (int)be16(q + 2), tot = (int)be16(q + 4);
                if (ty[0] == 't') { t->track = idx; t->track_total = tot; } else { t->disc = idx; t->disc_total = tot; }
            }
        }
        pos += (long)sz;
    }
}

/* --- WAV (LIST/INFO) --- */

static void read_wav_tags(const unsigned char *b, long n, mi_tags *t)
{
    long pos = 12;
    while (pos + 8 <= n) {
        long sz = (long)le32(b + pos + 4);
        if (sz < 0 || pos + 8 + sz > n) break;
        if (same4(b + pos, "LIST") && sz >= 4 && same4(b + pos + 8, "INFO")) {
            long q = pos + 12, e = pos + 8 + sz;
            while (q + 8 <= e) {
                long s2 = (long)le32(b + q + 4);
                char tmp[256];
                outbuf o;
                long i;
                if (s2 < 0 || q + 8 + s2 > e) break;
                o.buf = tmp; o.cap = (int)sizeof tmp; o.len = 0;
                { long m = 0; while (m < s2 && b[q + 8 + m]) m++; bytes_to_utf8(b + q + 8, m, &o); }
                tmp[o.len] = 0;
                { char c2[256]; mi_clean_text(tmp, c2, (int)sizeof c2, 250); for (i = 0; i < 256; i++) tmp[i] = c2[i]; }
                if (same4(b + q, "INAM")) scopy(t->title, (int)sizeof t->title, tmp);
                else if (same4(b + q, "IART")) scopy(t->artist, (int)sizeof t->artist, tmp);
                else if (same4(b + q, "IPRD")) scopy(t->album, (int)sizeof t->album, tmp);
                else if (same4(b + q, "IGNR")) scopy(t->genre, (int)sizeof t->genre, tmp);
                else if (same4(b + q, "ICRD")) t->year = parse_year(tmp);
                q += 8 + s2 + (s2 & 1);
            }
        }
        pos += 8 + sz + (sz & 1);
    }
}

int mi_read_tags(const unsigned char *b, long n, mi_tags *t)
{
    int fmt;
    mi_tags_clear(t);
    if (!b || n < 12) return 0;
    fmt = mi_detect(b, n);
    if (fmt == MI_FMT_MP3) {
        if (id3v2_size(b, n) > 0) read_id3v2(b, n, t);
        read_id3v1(b, n, t);
    } else if (fmt == MI_FMT_M4A) {
        read_mp4_tags(b, n, t);
    } else if (fmt == MI_FMT_WAV) {
        read_wav_tags(b, n, t);
    }
    return count_tags(t);
}

/* ---------- parametry dźwięku ---------- */

static void mp4_audio(const unsigned char *b, long n, mi_audio *a)
{
    range moov, mvhd, trak, mdia, minf, stbl, stsd;
    if (!mp4_find(b, 0, n, (const unsigned char *)"moov", &moov)) return;
    if (mp4_find(b, moov.start, moov.end, (const unsigned char *)"mvhd", &mvhd) && mvhd.end - mvhd.start >= 20) {
        const unsigned char *p = b + mvhd.start;
        double ts, du;
        if (p[0] == 1 && mvhd.end - mvhd.start >= 32) { ts = (double)be32(p + 20); du = (double)be32(p + 24) * 4294967296.0 + (double)be32(p + 28); }
        else { ts = (double)be32(p + 12); du = (double)be32(p + 16); }
        if (ts > 0.0) a->duration_ms = (long)(du * 1000.0 / ts + 0.5);
    }
    /* pierwszy 'trak' z dźwiękiem: moov/trak/mdia/minf/stbl/stsd */
    {
        long pos = moov.start;
        while (mp4_find(b, pos, moov.end, (const unsigned char *)"trak", &trak)) {
            if (mp4_find(b, trak.start, trak.end, (const unsigned char *)"mdia", &mdia)
                && mp4_find(b, mdia.start, mdia.end, (const unsigned char *)"minf", &minf)
                && mp4_find(b, minf.start, minf.end, (const unsigned char *)"stbl", &stbl)
                && mp4_find(b, stbl.start, stbl.end, (const unsigned char *)"stsd", &stsd)
                && stsd.end - stsd.start >= 8 + 36) {
                const unsigned char *e = b + stsd.start + 8;       /* pierwszy wpis */
                a->channels = (int)be16(e + 24);
                a->sample_rate = (int)be16(e + 32);
                break;
            }
            pos = trak.end;
        }
    }
    if (a->duration_ms > 0) a->bitrate_kbps = (int)((double)n * 8.0 / (double)a->duration_ms + 0.5);
}

static void wav_audio(const unsigned char *b, long n, mi_audio *a)
{
    long pos = 12;
    unsigned int byte_rate = 0;
    while (pos + 8 <= n) {
        long sz = (long)le32(b + pos + 4);
        if (same4(b + pos, "fmt ") && sz >= 16 && pos + 8 + 16 <= n) {
            a->channels = (int)le16(b + pos + 10);
            a->sample_rate = (int)le32(b + pos + 12);
            byte_rate = le32(b + pos + 16);
        } else if (same4(b + pos, "data")) {
            long dsz = sz;
            if (dsz < 0 || pos + 8 + dsz > n) dsz = n - pos - 8;
            if (byte_rate > 0) a->duration_ms = (long)((double)dsz * 1000.0 / (double)byte_rate + 0.5);
            break;
        }
        if (sz < 0 || pos + 8 + sz > n) break;
        pos += 8 + sz + (sz & 1);
    }
    if (byte_rate > 0) a->bitrate_kbps = (int)((double)byte_rate * 8.0 / 1000.0 + 0.5);
}

static void mp3_audio(const unsigned char *b, long n, mi_audio *a)
{
    long off = id3v2_size(b, n), fr;
    mp3hdr h;
    long tail = 0;
    fr = find_mp3_frame(b, n, off, &h);
    if (fr < 0) return;
    a->sample_rate = h.sr;
    a->channels = h.mono ? 1 : 2;
    a->bitrate_kbps = h.br;
    {   /* Xing / Info / VBRI */
        long x = fr + 4 + (h.ver == 0 ? (h.mono ? 17 : 32) : (h.mono ? 9 : 17));
        if (x + 12 <= n && (same4(b + x, "Xing") || same4(b + x, "Info")) && (b[x + 7] & 1)) {
            double frames = (double)be32(b + x + 8);
            if (frames > 0.0) {
                a->duration_ms = (long)(frames * h.spf * 1000.0 / h.sr + 0.5);
                a->bitrate_kbps = (int)((double)(n - fr) * 8.0 / (double)a->duration_ms + 0.5);
                return;
            }
        }
        x = fr + 4 + 32;
        if (x + 18 <= n && same4(b + x, "VBRI")) {
            double frames = (double)be32(b + x + 14);
            if (frames > 0.0) {
                a->duration_ms = (long)(frames * h.spf * 1000.0 / h.sr + 0.5);
                a->bitrate_kbps = (int)((double)(n - fr) * 8.0 / (double)a->duration_ms + 0.5);
                return;
            }
        }
    }
    if (n >= 128 && b[n - 128] == 'T' && b[n - 127] == 'A' && b[n - 126] == 'G') tail = 128;
    if (h.br > 0) a->duration_ms = (long)((double)(n - fr - tail) * 8.0 / (double)h.br + 0.5);
}

int mi_read_audio(const unsigned char *b, long n, mi_audio *a)
{
    a->format = MI_FMT_UNKNOWN; a->duration_ms = 0; a->bitrate_kbps = 0; a->sample_rate = 0; a->channels = 0;
    if (!b || n < 12) return 0;
    a->format = mi_detect(b, n);
    if (a->format == MI_FMT_MP3) mp3_audio(b, n, a);
    else if (a->format == MI_FMT_M4A) mp4_audio(b, n, a);
    else if (a->format == MI_FMT_WAV) wav_audio(b, n, a);
    return a->format != MI_FMT_UNKNOWN;
}

/* ---------- nazwa pliku ---------- */

void mi_tags_from_filename(const char *fname, mi_tags *t)
{
    char base[300], tmp[300];
    long len, i, dot = -1, slash = -1, start = 0, sep = -1;
    if (!fname) return;
    len = slen(fname);
    for (i = 0; i < len; i++) { if (fname[i] == '/') slash = i; }
    for (i = slash + 1; i < len; i++) { if (fname[i] == '.') dot = i; }
    start = slash + 1;
    if (dot < start) dot = len;
    if (dot - start <= 0) return;
    if (dot - start > 280) dot = start + 280;
    for (i = start; i < dot; i++) base[i - start] = (fname[i] == '_') ? ' ' : fname[i];
    base[dot - start] = 0;
    mi_clean_text(base, tmp, (int)sizeof tmp, 280);
    scopy(base, (int)sizeof base, tmp);
    /* początkowy numer utworu: "01 - ", "01. ", "01 " */
    {
        int k = 0;
        while (is_digit((unsigned char)base[k]) && k < 3) k++;
        if (k > 0 && k <= 3) {
            int j = k;
            if (base[j] == '.' || base[j] == ')') j++;
            if (base[j] == ' ') {
                while (base[j] == ' ') j++;
                if (base[j] == '-' && base[j + 1] == ' ') { j++; while (base[j] == ' ') j++; }
                if (base[j]) { scopy(tmp, (int)sizeof tmp, base + j); scopy(base, (int)sizeof base, tmp); }
            }
        }
    }
    len = slen(base);
    for (i = 0; i + 2 < len; i++) if (base[i] == ' ' && base[i + 1] == '-' && base[i + 2] == ' ') { sep = i; break; }
    if (sep > 0 && sep + 3 < len) {
        base[sep] = 0;
        if (!t->artist[0]) scopy(t->artist, (int)sizeof t->artist, base);
        if (!t->title[0]) scopy(t->title, (int)sizeof t->title, base + sep + 3);
    } else if (!t->title[0]) {
        scopy(t->title, (int)sizeof t->title, base);
    }
}

/* ---------- ścieżka w folderze Muzyki ---------- */

int mi_music_path(unsigned int r1, unsigned int r2, const char *ext, char *out, int n)
{
    int i = 0, k;
    unsigned int dir = r1 % 50u;
    if (n < 16) return 0;
    out[i++] = 'F';
    out[i++] = (char)('0' + (int)(dir / 10u));
    out[i++] = (char)('0' + (int)(dir % 10u));
    out[i++] = '/';
    for (k = 0; k < 4; k++) {
        out[i++] = (char)('A' + (int)(r2 % 26u));
        r2 = (r2 >> 5) | (r2 << 27);
        r2 = r2 * 1664525u + 1013904223u;
    }
    if (ext && ext[0]) {
        out[i++] = '.';
        for (k = 0; ext[k] && i < n - 1; k++) out[i++] = (char)lower((unsigned char)ext[k]);
    }
    out[i] = 0;
    return i;
}
