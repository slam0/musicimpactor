#import <UIKit/UIKit.h>

/* Ustawienia, kolejka i lista zaimportowanych utworów w Documents/musicimpactor.json.
   Pliki kolejki leżą w Documents/Kolejka/<id>.<rozszerzenie>, kopie bazy Muzyki w Documents/Kopie/. */

extern NSString *const MIQueueDidChangeNotification;      /* zmiana w kolejce albo postęp pobierania */

@interface MIStore : NSObject

+ (MIStore *)shared;

@property (nonatomic) NSInteger languageSetting;        /* 0 auto, 1 pl, 2 en, 3 ru */
@property (nonatomic) NSInteger themeIndex;
@property (nonatomic) BOOL removeAfterImport;           /* po imporcie usuń plik z kolejki */
@property (nonatomic) BOOL restartMusic;                /* po imporcie zamknij aplikację Muzyka, żeby wczytała bibliotekę */

@property (nonatomic, copy) NSString *lastReport;       /* ostatni raport z importu (zapisywany też w Documents/raport.txt) */

- (NSString *)documents;
- (NSString *)queueDir;
- (NSString *)backupDir;

/* ---- kolejka: słowniki (klucze: id, name, title, artist, album, albumartist, genre, year, track, tracks, disc, discs,
        fmt, ext, size, dur, br, sr, ch, status = ready / downloading / error / done, progress, error, url, added) ---- */
- (NSArray *)queue;
- (NSDictionary *)itemWithId:(NSString *)iid;
- (NSString *)pathForItem:(NSDictionary *)item;
- (NSUInteger)readyCount;

/* Kopiuje (albo przenosi) plik do kolejki, rozpoznaje format, czyta tagi. Zwraca element albo nil z opisem błędu. */
- (NSDictionary *)addFileAtPath:(NSString *)path displayName:(NSString *)name move:(BOOL)move error:(NSString **)err;
/* Element "pobiera się" - plik dojdzie później (patrz finishDownload...). */
- (NSString *)addDownloadingItemForURL:(NSString *)url name:(NSString *)name;
- (void)finishDownloadId:(NSString *)iid tempPath:(NSString *)tmp suggestedName:(NSString *)name error:(NSString *)err;
- (void)updateItemId:(NSString *)iid values:(NSDictionary *)values save:(BOOL)save;
- (void)deleteItemId:(NSString *)iid;
- (void)clearQueue;

/* ---- utwory dodane do Muzyki przez tę aplikację: {pid, rel, title, artist, album, added, size} ---- */
- (NSArray *)imported;
- (void)addImported:(NSDictionary *)rec;
- (void)removeImportedPid:(NSString *)pid;

- (void)save;

@end
