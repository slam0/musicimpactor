/* Testy rdzenia Music Impactor (mi_core.c) - uruchamiane na komputerze i w GitHub Actions. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../mi_core.h"

static int fails = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; printf("BŁĄD (linia %d): %s\n", __LINE__, #cond); } } while (0)
#define CHECK_STR(a, b) do { checks++; if (strcmp((a), (b)) != 0) { fails++; printf("BŁĄD (linia %d): \"%s\" != \"%s\"\n", __LINE__, (a), (b)); } } while (0)

typedef struct { unsigned char *p; long n, cap; } buf;
static void bput(buf *b, const void *d, long n)
{
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 64; b->p = (unsigned char *)realloc(b->p, (size_t)b->cap); }
    memcpy(b->p + b->n, d, (size_t)n);
    b->n += n;
}
static void bstr(buf *b, const char *s) { bput(b, s, (long)strlen(s)); }
static void b8(buf *b, int v) { unsigned char c = (unsigned char)v; bput(b, &c, 1); }
static void b16(buf *b, unsigned v) { b8(b, (int)(v >> 8)); b8(b, (int)(v & 255)); }
static void b24(buf *b, unsigned v) { b8(b, (int)(v >> 16)); b16(b, v & 0xFFFF); }
static void b32(buf *b, unsigned v) { b16(b, v >> 16); b16(b, v & 0xFFFF); }
static void l16(buf *b, unsigned v) { b8(b, (int)(v & 255)); b8(b, (int)(v >> 8)); }
static void l32(buf *b, unsigned v) { l16(b, v & 0xFFFF); l16(b, v >> 16); }
static void bzero_n(buf *b, long n) { long i; for (i = 0; i < n; i++) b8(b, 0); }
static void bfree(buf *b) { free(b->p); b->p = 0; b->n = b->cap = 0; }

static void syncsafe_put(buf *b, unsigned v) { b8(b, (int)((v >> 21) & 0x7F)); b8(b, (int)((v >> 14) & 0x7F)); b8(b, (int)((v >> 7) & 0x7F)); b8(b, (int)(v & 0x7F)); }

/* ramka ID3v2.3: id, kodowanie, dane */
static void frame23(buf *b, const char *id, int enc, const unsigned char *d, long n)
{
    bstr(b, id); b32(b, (unsigned)(n + 1)); b16(b, 0); b8(b, enc); bput(b, d, n);
}
static void frame24(buf *b, const char *id, int enc, const char *utf8)
{
    long n = (long)strlen(utf8);
    bstr(b, id); syncsafe_put(b, (unsigned)(n + 1)); b16(b, 0); b8(b, enc); bput(b, utf8, n);
}
static void id3_wrap(buf *out, int ver, const buf *frames)
{
    bstr(out, "ID3"); b8(out, ver); b8(out, 0); b8(out, 0);
    syncsafe_put(out, (unsigned)frames->n);
    bput(out, frames->p, frames->n);
}
static void mp3_frames(buf *b, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        long start = b->n;
        b8(b, 0xFF); b8(b, 0xFB); b8(b, 0x90); b8(b, 0x00);
        bzero_n(b, 417 - 4);
        (void)start;
    }
}

static void test_clean(void)
{
    char o[64];
    mi_clean_text("  Hello \t  world\n", o, sizeof o, 60); CHECK_STR(o, "Hello world");
    mi_clean_text("", o, sizeof o, 60); CHECK_STR(o, "");
    mi_clean_text("ąęść", o, sizeof o, 5); CHECK_STR(o, "ąę");              /* nie tnie znaku w połowie */
    mi_clean_text("abc", o, 3, 60); CHECK_STR(o, "ab");
}

static void test_detect(void)
{
    buf b = {0, 0, 0};
    unsigned char mp3[] = { 0xFF, 0xFB, 0x90, 0x00 };
    CHECK(mi_detect((const unsigned char *)"fLaC\0\0\0\0\0\0\0\0", 12) == MI_FMT_FLAC);
    CHECK(mi_detect((const unsigned char *)"OggS\0\0\0\0\0\0\0\0", 12) == MI_FMT_OGG);
    CHECK(mi_detect((const unsigned char *)"\0\0\0\x20" "ftypM4A \0\0\0\0", 16) == MI_FMT_M4A);
    CHECK(mi_detect((const unsigned char *)"RIFF\0\0\0\0WAVEfmt ", 16) == MI_FMT_WAV);
    CHECK(mi_detect((const unsigned char *)"<!DOCTYPE html><html>", 21) == MI_FMT_HTML);
    CHECK(mi_detect((const unsigned char *)"hello world, not audio", 22) == MI_FMT_UNKNOWN);
    mp3_frames(&b, 3);
    CHECK(mi_detect(b.p, b.n) == MI_FMT_MP3);
    CHECK(mi_detect(mp3, 4) == MI_FMT_UNKNOWN || mi_detect(mp3, 4) == MI_FMT_MP3);      /* za krótkie: nie wywala się */
    CHECK(mi_format_supported(MI_FMT_MP3) && mi_format_supported(MI_FMT_M4A) && mi_format_supported(MI_FMT_WAV));
    CHECK(!mi_format_supported(MI_FMT_FLAC) && !mi_format_supported(MI_FMT_OGG) && !mi_format_supported(MI_FMT_UNKNOWN));
    CHECK_STR(mi_ext_for_format(MI_FMT_M4A), "m4a");
    bfree(&b);
}

static void test_mp3_audio(void)
{
    buf b = {0, 0, 0}, fr = {0, 0, 0}, f;
    mi_audio a;
    mp3_frames(&b, 100);
    CHECK(mi_read_audio(b.p, b.n, &a) == 1);
    CHECK(a.format == MI_FMT_MP3 && a.bitrate_kbps == 128 && a.sample_rate == 44100 && a.channels == 2);
    CHECK(a.duration_ms == 2606);
    bfree(&b);

    /* z tagiem ID3v2 z przodu i z nagłówkiem Xing (200 ramek) */
    bzero_n(&fr, 100);
    f.p = 0; f.n = f.cap = 0;
    bstr(&f, "TIT2"); b32(&f, 6); b16(&f, 0); b8(&f, 3); bstr(&f, "Test");
    id3_wrap(&b, 3, &f);
    {
        long i;
        b8(&b, 0xFF); b8(&b, 0xFB); b8(&b, 0x90); b8(&b, 0x00);
        bzero_n(&b, 32);
        bstr(&b, "Xing"); b32(&b, 1); b32(&b, 200);
        bzero_n(&b, 417 - 4 - 32 - 12);
        for (i = 0; i < 5; i++) { b8(&b, 0xFF); b8(&b, 0xFB); b8(&b, 0x90); b8(&b, 0x00); bzero_n(&b, 413); }
    }
    CHECK(mi_read_audio(b.p, b.n, &a) == 1);
    CHECK(a.duration_ms == 5224);
    bfree(&b); bfree(&fr); bfree(&f);

    /* MPEG2 warstwa III, 64 kb/s, 22050 Hz, mono: nagłówek FF F3 80 C0 */
    {
        long i;
        for (i = 0; i < 40; i++) { b8(&b, 0xFF); b8(&b, 0xF3); b8(&b, 0x80); b8(&b, 0xC0); bzero_n(&b, 208 - 4); }
        CHECK(mi_read_audio(b.p, b.n, &a) == 1);
        CHECK(a.sample_rate == 22050 && a.channels == 1 && a.bitrate_kbps == 64);
        CHECK(a.duration_ms > 1000 && a.duration_ms < 1400);
        bfree(&b);
    }
}

static void test_id3(void)
{
    buf fr = {0, 0, 0}, out = {0, 0, 0}, tail = {0, 0, 0};
    mi_tags t;
    /* UTF-16 z BOM: "Zażółć" */
    const unsigned char u16[] = { 0xFF, 0xFE, 'Z', 0, 'a', 0, 0x7C, 0x01, 0xF3, 0x00, 0x42, 0x01, 0x07, 0x01 };
    /* Windows-1250 w polu "Latin-1": "Łódź" = 0xA3 0xF3 0x64 0x9F */
    const unsigned char cp[] = { 0xA3, 0xF3, 'd', 0x9F };
    mp3_frames(&tail, 20);
    frame23(&fr, "TIT2", 1, u16, (long)sizeof u16);
    frame23(&fr, "TPE1", 0, cp, (long)sizeof cp);
    frame23(&fr, "TALB", 0, (const unsigned char *)"Album Test", 10);
    frame23(&fr, "TRCK", 0, (const unsigned char *)"3/12", 4);
    frame23(&fr, "TPOS", 0, (const unsigned char *)"1/2", 3);
    frame23(&fr, "TYER", 0, (const unsigned char *)"2012", 4);
    frame23(&fr, "TCON", 0, (const unsigned char *)"(17)", 4);
    frame23(&fr, "TPE2", 0, (const unsigned char *)"Various", 7);
    id3_wrap(&out, 3, &fr);
    bput(&out, tail.p, tail.n);
    CHECK(mi_read_tags(out.p, out.n, &t) >= 8);
    CHECK_STR(t.title, "Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87");               /* Zażółć */
    CHECK_STR(t.artist, "\xC5\x81\xC3\xB3" "d\xC5\xBA");                    /* Łódź */
    CHECK_STR(t.album, "Album Test");
    CHECK(t.track == 3 && t.track_total == 12 && t.disc == 1 && t.disc_total == 2);
    CHECK(t.year == 2012);
    CHECK_STR(t.genre, "Rock");
    CHECK_STR(t.albumartist, "Various");
    bfree(&fr); bfree(&out);

    /* ID3v2.4: UTF-8, TDRC z pełną datą, gatunek tekstem */
    frame24(&fr, "TIT2", 3, "Pi\xC4\x99kna");
    frame24(&fr, "TPE1", 3, "Artysta");
    frame24(&fr, "TDRC", 3, "2019-05-17");
    frame24(&fr, "TCON", 3, "Hip-Hop");
    id3_wrap(&out, 4, &fr);
    bput(&out, tail.p, tail.n);
    CHECK(mi_read_tags(out.p, out.n, &t) >= 4);
    CHECK_STR(t.title, "Pi\xC4\x99kna");
    CHECK(t.year == 2019);
    CHECK_STR(t.genre, "Hip-Hop");
    bfree(&fr); bfree(&out);

    /* ID3v2.2 (trzyznakowe identyfikatory) */
    {
        const char *s = "Old";
        bstr(&fr, "TT2"); b24(&fr, 4); b8(&fr, 0); bstr(&fr, s);
        bstr(&fr, "TP1"); b24(&fr, 4); b8(&fr, 0); bstr(&fr, "Dud");
        bstr(&out, "ID3"); b8(&out, 2); b8(&out, 0); b8(&out, 0); syncsafe_put(&out, (unsigned)fr.n); bput(&out, fr.p, fr.n);
        bput(&out, tail.p, tail.n);
        CHECK(mi_read_tags(out.p, out.n, &t) == 2);
        CHECK_STR(t.title, "Old"); CHECK_STR(t.artist, "Dud");
        bfree(&fr); bfree(&out);
    }

    /* ID3v1 na końcu pliku bez ID3v2 */
    {
        unsigned char v1[128];
        memset(v1, 0, sizeof v1);
        memcpy(v1, "TAG", 3); memcpy(v1 + 3, "Tytul v1", 8); memcpy(v1 + 33, "Wykonawca", 9); memcpy(v1 + 63, "Plyta", 5);
        memcpy(v1 + 93, "1999", 4); v1[125] = 0; v1[126] = 7; v1[127] = 13;
        bput(&out, tail.p, tail.n); bput(&out, v1, 128);
        CHECK(mi_read_tags(out.p, out.n, &t) >= 5);
        CHECK_STR(t.title, "Tytul v1"); CHECK_STR(t.artist, "Wykonawca"); CHECK_STR(t.album, "Plyta");
        CHECK(t.year == 1999 && t.track == 7);
        CHECK_STR(t.genre, "Pop");
        bfree(&out);
    }
    /* plik bez tagów */
    bput(&out, tail.p, tail.n);
    CHECK(mi_read_tags(out.p, out.n, &t) == 0);
    bfree(&out); bfree(&tail);
}

static void atom_begin(buf *b, const char *type, long *mark) { *mark = b->n; b32(b, 0); bstr(b, type); }
static void atom_end(buf *b, long mark) { unsigned v = (unsigned)(b->n - mark); b->p[mark] = (unsigned char)(v >> 24); b->p[mark + 1] = (unsigned char)(v >> 16); b->p[mark + 2] = (unsigned char)(v >> 8); b->p[mark + 3] = (unsigned char)v; }
static void text_item(buf *b, const unsigned char *type4, const char *text)
{
    long m1, m2;
    m1 = b->n; b32(b, 0); bput(b, type4, 4);
    m2 = b->n; b32(b, 0); bstr(b, "data"); b32(b, 1); b32(b, 0); bstr(b, text); atom_end(b, m2);
    atom_end(b, m1);
}

static void test_mp4(void)
{
    buf b = {0, 0, 0};
    long ftyp, moov, mvhd, udta, meta, ilst, trkn, data, trak, mdia, minf, stbl, stsd;
    mi_tags t;
    mi_audio a;
    atom_begin(&b, "ftyp", &ftyp); bstr(&b, "M4A "); b32(&b, 0); bstr(&b, "M4A mp42isom"); atom_end(&b, ftyp);
    atom_begin(&b, "moov", &moov);
      atom_begin(&b, "mvhd", &mvhd); b32(&b, 0); b32(&b, 0); b32(&b, 0); b32(&b, 1000); b32(&b, 215000); bzero_n(&b, 80); atom_end(&b, mvhd);
      atom_begin(&b, "trak", &trak);
       atom_begin(&b, "mdia", &mdia);
        atom_begin(&b, "minf", &minf);
         atom_begin(&b, "stbl", &stbl);
          atom_begin(&b, "stsd", &stsd); b32(&b, 0); b32(&b, 1);
           b32(&b, 36 + 8); bstr(&b, "mp4a"); bzero_n(&b, 6); b16(&b, 1); bzero_n(&b, 8); b16(&b, 2); b16(&b, 16); b16(&b, 0); b16(&b, 0); b16(&b, 44100); b16(&b, 0);
          atom_end(&b, stsd);
         atom_end(&b, stbl);
        atom_end(&b, minf);
       atom_end(&b, mdia);
      atom_end(&b, trak);
      atom_begin(&b, "udta", &udta);
       atom_begin(&b, "meta", &meta); b32(&b, 0);
        atom_begin(&b, "ilst", &ilst);
         text_item(&b, (const unsigned char *)"\xA9" "nam", "Tytu\xC5\x82 m4a");
         text_item(&b, (const unsigned char *)"\xA9" "ART", "Wykonawca m4a");
         text_item(&b, (const unsigned char *)"\xA9" "alb", "Album m4a");
         text_item(&b, (const unsigned char *)"\xA9" "gen", "Jazz");
         text_item(&b, (const unsigned char *)"\xA9" "day", "2008-01-02");
         text_item(&b, (const unsigned char *)"aART", "Zespol");
         trkn = b.n; b32(&b, 0); bstr(&b, "trkn");
         data = b.n; b32(&b, 0); bstr(&b, "data"); b32(&b, 0); b32(&b, 0); b16(&b, 0); b16(&b, 5); b16(&b, 11); b16(&b, 0); atom_end(&b, data);
         atom_end(&b, trkn);
        atom_end(&b, ilst);
       atom_end(&b, meta);
      atom_end(&b, udta);
    atom_end(&b, moov);
    bstr(&b, "mdat");  /* niby dane */
    CHECK(mi_detect(b.p, b.n) == MI_FMT_M4A);
    CHECK(mi_read_tags(b.p, b.n, &t) >= 7);
    CHECK_STR(t.title, "Tytu\xC5\x82 m4a"); CHECK_STR(t.artist, "Wykonawca m4a"); CHECK_STR(t.album, "Album m4a");
    CHECK_STR(t.genre, "Jazz"); CHECK(t.year == 2008); CHECK_STR(t.albumartist, "Zespol");
    CHECK(t.track == 5 && t.track_total == 11);
    CHECK(mi_read_audio(b.p, b.n, &a) == 1);
    CHECK(a.format == MI_FMT_M4A && a.duration_ms == 215000 && a.sample_rate == 44100 && a.channels == 2);
    bfree(&b);
}

static void test_wav(void)
{
    buf b = {0, 0, 0};
    mi_audio a;
    mi_tags t;
    long list;
    bstr(&b, "RIFF"); l32(&b, 0); bstr(&b, "WAVE");
    bstr(&b, "fmt "); l32(&b, 16); l16(&b, 1); l16(&b, 2); l32(&b, 44100); l32(&b, 176400); l16(&b, 4); l16(&b, 16);
    bstr(&b, "LIST"); list = b.n; l32(&b, 0); bstr(&b, "INFO"); bstr(&b, "INAM"); l32(&b, 4); bstr(&b, "Wav"); b8(&b, 0); bstr(&b, "IART"); l32(&b, 4); bstr(&b, "Art"); b8(&b, 0);
    { unsigned v = (unsigned)(b.n - list - 4); b.p[list] = (unsigned char)v; b.p[list + 1] = 0; b.p[list + 2] = 0; b.p[list + 3] = 0; }
    bstr(&b, "data"); l32(&b, 352800); bzero_n(&b, 352800);
    CHECK(mi_read_audio(b.p, b.n, &a) == 1);
    CHECK(a.format == MI_FMT_WAV && a.duration_ms == 2000 && a.sample_rate == 44100 && a.channels == 2 && a.bitrate_kbps == 1411);
    CHECK(mi_read_tags(b.p, b.n, &t) == 2);
    CHECK_STR(t.title, "Wav"); CHECK_STR(t.artist, "Art");
    bfree(&b);
}

static void test_filename(void)
{
    mi_tags t;
    mi_tags_clear(&t); mi_tags_from_filename("01 - Metallica - One.mp3", &t);
    CHECK_STR(t.artist, "Metallica"); CHECK_STR(t.title, "One");
    mi_tags_clear(&t); mi_tags_from_filename("Daft_Punk - Get_Lucky.m4a", &t);
    CHECK_STR(t.artist, "Daft Punk"); CHECK_STR(t.title, "Get Lucky");
    mi_tags_clear(&t); mi_tags_from_filename("/var/mobile/Documents/sam tytul.mp3", &t);
    CHECK_STR(t.artist, ""); CHECK_STR(t.title, "sam tytul");
    mi_tags_clear(&t); mi_tags_from_filename("07. Piosenka.wav", &t);
    CHECK_STR(t.title, "Piosenka");
    mi_tags_clear(&t); strcpy(t.title, "Zostaje"); mi_tags_from_filename("A - B.mp3", &t);
    CHECK_STR(t.title, "Zostaje"); CHECK_STR(t.artist, "A");
    mi_tags_clear(&t); mi_tags_from_filename("2pac - Changes.mp3", &t);
    CHECK_STR(t.artist, "2pac"); CHECK_STR(t.title, "Changes");
    mi_tags_clear(&t); mi_tags_from_filename("", &t);
    CHECK_STR(t.title, "");
}

static void test_path(void)
{
    char p[32];
    int i;
    for (i = 0; i < 2000; i++) {
        unsigned r1 = (unsigned)rand() * 2654435761u, r2 = (unsigned)rand() * 40503u + (unsigned)i;
        int n = mi_music_path(r1, r2, "MP3", p, sizeof p);
        CHECK(n == 12);
        CHECK(p[0] == 'F' && p[1] >= '0' && p[1] <= '4' && p[2] >= '0' && p[2] <= '9' && p[3] == '/');
        CHECK(p[4] >= 'A' && p[4] <= 'Z' && p[7] >= 'A' && p[7] <= 'Z' && p[8] == '.');
        CHECK(strcmp(p + 8, ".mp3") == 0);
    }
    CHECK(mi_music_path(0, 0, "m4a", p, 8) == 0);
}

int main(void)
{
    test_clean();
    test_detect();
    test_mp3_audio();
    test_id3();
    test_mp4();
    test_wav();
    test_filename();
    test_path();
    printf("sprawdzeń: %d, błędów: %d\n", checks, fails);
    return fails ? 1 : 0;
}
