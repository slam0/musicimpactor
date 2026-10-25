/*
 * Music Impactor - logika plików audio w czystym C (bez biblioteki standardowej poza string.h-podobnymi
 * funkcjami własnymi), osobno, żeby dało się ją przetestować na komputerze.
 *
 * Czyta tagi (ID3v2, ID3v1, MP4/M4A, WAV INFO), długość i parametry dźwięku oraz układa
 * nazwę pliku w folderze Muzyki. Nie używa dzielenia całkowitego ani liczb 64-bitowych
 * (na iOS 6 w zastępczej bibliotece nie ma na nie pomocniczych funkcji).
 */
#ifndef MI_CORE_H
#define MI_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MI_FMT_UNKNOWN = 0,
    MI_FMT_MP3 = 1,
    MI_FMT_M4A = 2,
    MI_FMT_WAV = 3,
    /* poniższe rozpoznajemy tylko po to, żeby napisać, że iOS 6 ich nie odtworzy */
    MI_FMT_FLAC = 10,
    MI_FMT_OGG = 11,
    MI_FMT_AIFF = 12,
    MI_FMT_WMA = 13,
    MI_FMT_HTML = 14        /* to nie plik audio, tylko strona www (np. zły link) */
};

typedef struct {
    char title[256];
    char artist[256];
    char album[256];
    char albumartist[256];
    char genre[128];
    int year;               /* 0 = brak */
    int track, track_total; /* 0 = brak */
    int disc, disc_total;
} mi_tags;

typedef struct {
    int format;
    long duration_ms;       /* 0 = nieznana */
    int bitrate_kbps;       /* 0 = nieznany */
    int sample_rate;        /* Hz, 0 = nieznana */
    int channels;           /* 0 = nieznana */
} mi_audio;

/* Rozpoznaje format po początku pliku (wystarczy kilkanaście bajtów; dla MP3 najlepiej 64 KB). */
int mi_detect(const unsigned char *b, long n);

/* Czy iOS 6 odtworzy ten format (MP3, M4A, WAV). */
int mi_format_supported(int fmt);

/* Rozszerzenie pliku dla formatu ("mp3", "m4a", "wav") albo "" gdy nieznany. */
const char *mi_ext_for_format(int fmt);

/* Zeruje strukturę tagów. */
void mi_tags_clear(mi_tags *t);

/* Czyta tagi z całego pliku w pamięci. Zwraca liczbę znalezionych pól tekstowych (tytuł, wykonawca, album, gatunek, rok, numer...). */
int mi_read_tags(const unsigned char *b, long n, mi_tags *t);

/* Czyta długość i parametry dźwięku. Zwraca 1, gdy udało się ustalić format, 0 gdy nie. */
int mi_read_audio(const unsigned char *b, long n, mi_audio *a);

/* Uzupełnia puste pola tytułu i wykonawcy na podstawie nazwy pliku ("01 - Wykonawca - Tytuł.mp3"). */
void mi_tags_from_filename(const char *fname, mi_tags *t);

/* Czyści tekst: obcina spacje z brzegów, zamienia białe znaki na pojedyncze spacje,
   skraca do max_bytes bajtów (nie tnie znaku UTF-8). Zwraca długość wyniku. */
int mi_clean_text(const char *in, char *out, int n, int max_bytes);

/* Nazwa pliku w folderze Muzyki iOS: "F07/ABCD.mp3" (F00-F49, cztery wielkie litery). r1 i r2 to liczby losowe. */
int mi_music_path(unsigned int r1, unsigned int r2, const char *ext, char *out, int n);

/* Zamienia ID3v1 numer gatunku (0-79) na nazwę albo NULL. */
const char *mi_genre_name(int idx);

#ifdef __cplusplus
}
#endif
#endif
