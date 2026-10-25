/*
 * Music Impactor - zapis utworów do biblioteki aplikacji Muzyka (iOS 6) w czystym C + SQLite.
 *
 * Schematu bazy MediaLibrary.sqlitedb nie znamy na pewno, dlatego kod jest "adaptacyjny":
 *  - czyta, jakie tabele i kolumny w bazie naprawdę są (PRAGMA table_info),
 *  - jako wzór bierze wiersze jednego istniejącego utworu i klonuje je z podmienionymi polami,
 *  - przed zapisem robi kopię bazy, pracuje w jednej transakcji (na błąd: wycofanie) i sprawdza spójność,
 *  - o wszystkim pisze w raporcie, który da się skopiować.
 * Dzięki temu da się go przetestować na sztucznych bazach na komputerze.
 *
 * Uwaga (iOS 6, armv7): bez dzielenia i reszty z dzielenia na liczbach 64-bitowych ani zmiennych dzielników.
 */
#ifndef MI_LIB_H
#define MI_LIB_H

#include "mi_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *src_path;               /* plik do skopiowania do folderu Muzyki */
    const char *title, *artist, *album, *albumartist, *genre;
    int year, track, track_total, disc, disc_total;
    int format;                         /* MI_FMT_* */
    long duration_ms;
    int bitrate_kbps, sample_rate, channels;
} mi_lib_track;

/* wiersz w tabeli pomocniczej (wykonawca, album...), który utworzyliśmy - do sprzątania przy usuwaniu */
typedef struct {
    char table[24];
    char keycol[24];
    char refcol[24];                    /* kolumna w tabeli item, która na niego wskazuje */
    long long key;
} mi_lib_entity;

typedef struct {
    int ok;
    long long pid;
    char rel[24];                       /* np. "F07/ABCD.mp3" */
    char error[300];
    mi_lib_entity created[5];
    int ncreated;
} mi_lib_result;

/* Zwraca liczbę zaimportowanych utworów albo -1 przy błędzie ogólnym (wtedy w raporcie jest przyczyna).
   res musi mieć n elementów. backup_dir może być NULL (bez kopii - tylko do testów). */
int mi_lib_import(const char *db_path, const char *backup_dir, const mi_lib_track *tracks, int n,
                  mi_lib_result *res, char *report, int cap);

/* Usuwa utwór dodany wcześniej (wiersze z bazy i plik). Zwraca 1 gdy się udało. */
int mi_lib_remove(const char *db_path, long long pid, const char *rel,
                  const mi_lib_entity *ents, int nent, char *report, int cap);

/* Opis bazy (tabele, kolumny, liczby wierszy, przykładowy utwór) do skopiowania i przysłania. */
int mi_lib_diagnose(const char *db_path, char *out, int cap);

/* Przywraca najnowszą kopię z backup_dir. Zwraca 1 gdy się udało. */
int mi_lib_restore_backup(const char *db_path, const char *backup_dir, char *report, int cap);

#ifdef __cplusplus
}
#endif
#endif
