/* Narzędzie testowe: lib_cli import|remove|diagnose|restore ... (patrz lib_test.py) */
#include "../mi_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char rep[200000];

static int make_src(const char *path, int fmt)
{
    FILE *f = fopen(path, "wb");
    unsigned char hdr[16] = { 'I','D','3',3,0,0,0,0,0,0 };
    int i;
    if (!f) return 0;
    if (fmt == MI_FMT_MP3) fwrite(hdr, 1, 10, f);
    for (i = 0; i < 4000; i++) fputc(i & 0xFF, f);
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    const char *cmd = argc > 1 ? argv[1] : "";
    const char *db = argc > 2 ? argv[2] : "";
    if (strcmp(cmd, "import") == 0) {
        const char *backup = argc > 3 ? argv[3] : NULL;
        const char *tmp = argc > 4 ? argv[4] : ".";
        mi_lib_track t[4];
        mi_lib_result res[4];
        char p[4][600];
        int i, n;
        memset(t, 0, sizeof t);
        for (i = 0; i < 4; i++) snprintf(p[i], sizeof p[i], "%s/src%d.bin", tmp, i);
        make_src(p[0], MI_FMT_MP3); make_src(p[1], MI_FMT_MP3); make_src(p[2], MI_FMT_M4A); make_src(p[3], MI_FMT_WAV);
        t[0].src_path = p[0]; t[0].title = "Zażółć gęślą jaźń"; t[0].artist = "Nowy Wykonawca"; t[0].album = "Nowy Album"; t[0].genre = "Rock";
        t[0].format = MI_FMT_MP3; t[0].duration_ms = 215000; t[0].bitrate_kbps = 192; t[0].sample_rate = 44100; t[0].track = 1; t[0].track_total = 2; t[0].year = 2021;
        t[1].src_path = p[1]; t[1].title = "Drugi"; t[1].artist = "Nowy Wykonawca"; t[1].album = "Nowy Album"; t[1].genre = "Rock";
        t[1].format = MI_FMT_MP3; t[1].duration_ms = 100000; t[1].bitrate_kbps = 128; t[1].sample_rate = 44100; t[1].track = 2; t[1].track_total = 2; t[1].year = 2021;
        t[2].src_path = p[2]; t[2].title = "M4A song"; t[2].artist = "Istniejący"; t[2].album = ""; t[2].genre = "";
        t[2].format = MI_FMT_M4A; t[2].duration_ms = 60000; t[2].bitrate_kbps = 256; t[2].sample_rate = 44100;
        t[3].src_path = p[3]; t[3].title = ""; t[3].artist = ""; t[3].album = "";
        t[3].format = MI_FMT_WAV; t[3].duration_ms = 1000; t[3].sample_rate = 44100;
        n = mi_lib_import(db, backup && strcmp(backup, "-") ? backup : NULL, t, 4, res, rep, (int)sizeof rep);
        printf("RETURN %d\n", n);
        for (i = 0; i < 4; i++) {
            int k;
            printf("RESULT %d %d %lld %s |%s|", i, res[i].ok, res[i].pid, res[i].rel, res[i].error);
            for (k = 0; k < res[i].ncreated; k++) printf(" ENT:%s:%s:%s:%lld", res[i].created[k].table, res[i].created[k].keycol, res[i].created[k].refcol, res[i].created[k].key);
            printf("\n");
        }
        printf("%s\n", rep);
        return 0;
    }
    if (strcmp(cmd, "remove") == 0) {
        long long pid = atoll(argv[3]);
        const char *rel = argv[4];
        mi_lib_entity e[5];
        int ne = 0, i;
        memset(e, 0, sizeof e);
        for (i = 5; i + 3 < argc && ne < 5; i += 4) {
            snprintf(e[ne].table, sizeof e[ne].table, "%s", argv[i]);
            snprintf(e[ne].keycol, sizeof e[ne].keycol, "%s", argv[i + 1]);
            snprintf(e[ne].refcol, sizeof e[ne].refcol, "%s", argv[i + 2]);
            e[ne].key = atoll(argv[i + 3]);
            ne++;
        }
        printf("RETURN %d\n", mi_lib_remove(db, pid, rel, e, ne, rep, (int)sizeof rep));
        printf("%s\n", rep);
        return 0;
    }
    if (strcmp(cmd, "diagnose") == 0) {
        printf("RETURN %d\n", mi_lib_diagnose(db, rep, (int)sizeof rep));
        printf("%s\n", rep);
        return 0;
    }
    if (strcmp(cmd, "restore") == 0) {
        printf("RETURN %d\n", mi_lib_restore_backup(db, argv[3], rep, (int)sizeof rep));
        printf("%s\n", rep);
        return 0;
    }
    fprintf(stderr, "?\n");
    return 2;
}
