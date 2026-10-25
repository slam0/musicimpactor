#import "MIStore.h"
#import "MILang.h"
#import "MIViews.h"
#import "mi_core.h"

NSString *const MIQueueDidChangeNotification = @"MIQueueDidChangeNotification";

@interface MIStore ()
{
    NSMutableDictionary *_root;
}
@end

@implementation MIStore

+ (MIStore *)shared
{
    static MIStore *s;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ s = [[MIStore alloc] init]; });
    return s;
}

- (NSString *)documents
{
    NSArray *dirs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    return [dirs objectAtIndex:0];
}

- (NSString *)lastReport
{
    NSString *s = [NSString stringWithContentsOfFile:[[self documents] stringByAppendingPathComponent:@"raport.txt"] encoding:NSUTF8StringEncoding error:NULL];
    return s;
}

- (void)setLastReport:(NSString *)r
{
    if (r) [r writeToFile:[[self documents] stringByAppendingPathComponent:@"raport.txt"] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
}

- (NSString *)dataPath { return [[self documents] stringByAppendingPathComponent:@"musicimpactor.json"]; }
- (NSString *)queueDir { return [[self documents] stringByAppendingPathComponent:@"Kolejka"]; }
- (NSString *)backupDir { return [[self documents] stringByAppendingPathComponent:@"Kopie"]; }

- (id)init
{
    if ((self = [super init])) {
        [[NSFileManager defaultManager] createDirectoryAtPath:[self queueDir] withIntermediateDirectories:YES attributes:nil error:NULL];
        [self load];
    }
    return self;
}

- (void)load
{
    NSData *d = [NSData dataWithContentsOfFile:[self dataPath]];
    id obj = nil;
    if (d) {
        obj = [NSJSONSerialization JSONObjectWithData:d options:NSJSONReadingMutableContainers error:NULL];
        if (![obj isKindOfClass:[NSMutableDictionary class]]) {
            NSString *bad = [[self dataPath] stringByAppendingString:@".uszkodzony"];
            [[NSFileManager defaultManager] removeItemAtPath:bad error:NULL];
            [[NSFileManager defaultManager] moveItemAtPath:[self dataPath] toPath:bad error:NULL];
            obj = nil;
        }
    }
    _root = obj ? obj : [NSMutableDictionary dictionary];
    [self ensureDefaults];
}

- (void)ensureDefaults
{
    NSDictionary *numbers = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithInt:0], @"lang", [NSNumber numberWithInt:0], @"theme",
        [NSNumber numberWithBool:YES], @"removeAfter", [NSNumber numberWithBool:YES], @"restartMusic", nil];
    for (NSString *k in numbers) {
        if (![[_root objectForKey:k] isKindOfClass:[NSNumber class]]) [_root setObject:[numbers objectForKey:k] forKey:k];
    }
    if (![[_root objectForKey:@"queue"] isKindOfClass:[NSArray class]]) [_root setObject:[NSMutableArray array] forKey:@"queue"];
    if (![[_root objectForKey:@"imported"] isKindOfClass:[NSArray class]]) [_root setObject:[NSMutableArray array] forKey:@"imported"];
    /* sprzątanie: pobieranie, które przerwało zamknięcie aplikacji, oraz elementy bez pliku */
    NSMutableArray *queue = [_root objectForKey:@"queue"];
    NSMutableArray *bad = [NSMutableArray array];
    for (id it in queue) {
        if (![it isKindOfClass:[NSMutableDictionary class]] || ![[it objectForKey:@"id"] isKindOfClass:[NSString class]]) { [bad addObject:it]; continue; }
        NSMutableDictionary *m = it;
        NSString *st = [m objectForKey:@"status"];
        if ([st isEqualToString:@"downloading"]) {
            [m setObject:@"error" forKey:@"status"];
            [m setObject:@"Pobieranie zostało przerwane" forKey:@"error"];   /* tłumaczone przy wyświetlaniu (GL tu wywołałoby MIStore rekurencyjnie) */
        } else if ([st isEqualToString:@"ready"] || [st isEqualToString:@"done"]) {
            if (![[NSFileManager defaultManager] fileExistsAtPath:[self pathForItem:m]]) [bad addObject:it];
        }
    }
    [queue removeObjectsInArray:bad];
}

- (void)save
{
    NSData *d = [NSJSONSerialization dataWithJSONObject:_root options:0 error:NULL];
    if (d) [d writeToFile:[self dataPath] atomically:YES];
}

- (void)notify
{
    [[NSNotificationCenter defaultCenter] postNotificationName:MIQueueDidChangeNotification object:nil];
}

- (NSInteger)languageSetting { return [[_root objectForKey:@"lang"] integerValue]; }
- (void)setLanguageSetting:(NSInteger)v { [_root setObject:[NSNumber numberWithInteger:v] forKey:@"lang"]; [self save]; }
- (NSInteger)themeIndex { return [[_root objectForKey:@"theme"] integerValue]; }
- (void)setThemeIndex:(NSInteger)v { [_root setObject:[NSNumber numberWithInteger:v] forKey:@"theme"]; [self save]; }
- (BOOL)removeAfterImport { return [[_root objectForKey:@"removeAfter"] boolValue]; }
- (void)setRemoveAfterImport:(BOOL)v { [_root setObject:[NSNumber numberWithBool:v] forKey:@"removeAfter"]; [self save]; }
- (BOOL)restartMusic { return [[_root objectForKey:@"restartMusic"] boolValue]; }
- (void)setRestartMusic:(BOOL)v { [_root setObject:[NSNumber numberWithBool:v] forKey:@"restartMusic"]; [self save]; }

/* ---------- kolejka ---------- */

- (NSArray *)queue { return [NSArray arrayWithArray:[_root objectForKey:@"queue"]]; }

- (NSMutableDictionary *)mutableItemWithId:(NSString *)iid
{
    if (!iid) return nil;
    for (NSMutableDictionary *it in [_root objectForKey:@"queue"]) if ([[it objectForKey:@"id"] isEqualToString:iid]) return it;
    return nil;
}

- (NSDictionary *)itemWithId:(NSString *)iid { return [self mutableItemWithId:iid]; }

- (NSString *)pathForItem:(NSDictionary *)item
{
    NSString *ext = [item objectForKey:@"ext"];
    NSString *name = [[item objectForKey:@"id"] stringByAppendingPathExtension:[ext length] ? ext : @"bin"];
    return [[self queueDir] stringByAppendingPathComponent:name];
}

- (NSUInteger)readyCount
{
    NSUInteger n = 0;
    for (NSDictionary *it in [_root objectForKey:@"queue"]) if ([[it objectForKey:@"status"] isEqualToString:@"ready"]) n++;
    return n;
}

static NSString *NewId(void)
{
    return [NSString stringWithFormat:@"%08x%08x", (unsigned)arc4random(), (unsigned)arc4random()];
}

static NSString *Clean(NSString *s, int maxBytes)
{
    if (!s) return @"";
    char buf[512];
    mi_clean_text([s UTF8String], buf, (int)sizeof buf, maxBytes);
    NSString *r = [NSString stringWithUTF8String:buf];
    return r ? r : @"";
}

/* rozpoznaje plik i uzupełnia elementowi dane; zwraca nil, gdy wszystko w porządku, albo opis błędu */
- (NSString *)analyzeItem:(NSMutableDictionary *)item
{
    NSString *path = [self pathForItem:item];
    NSData *data = [NSData dataWithContentsOfFile:path options:NSDataReadingMappedIfSafe error:NULL];
    if (!data || [data length] < 16) return GL(@"Plik jest pusty albo nie da się go odczytać");
    const unsigned char *b = (const unsigned char *)[data bytes];
    long n = (long)[data length];
    mi_audio a;
    mi_read_audio(b, n, &a);
    if (a.format == MI_FMT_HTML) return GL(@"To nie jest plik audio, tylko strona internetowa (sprawdź link)");
    if (!mi_format_supported(a.format)) {
        if (a.format == MI_FMT_FLAC || a.format == MI_FMT_OGG || a.format == MI_FMT_AIFF || a.format == MI_FMT_WMA)
            return [NSString stringWithFormat:GL(@"Format %@ nie jest obsługiwany przez Muzykę w iOS 6 (potrzebny MP3, M4A albo WAV)"), [[NSString stringWithUTF8String:mi_ext_for_format(a.format)] uppercaseString]];
        return GL(@"Nie rozpoznaję tego formatu (potrzebny MP3, M4A albo WAV)");
    }
    mi_tags t;
    mi_read_tags(b, n, &t);
    mi_tags_from_filename([[item objectForKey:@"name"] UTF8String], &t);
    [item setObject:[NSString stringWithUTF8String:mi_ext_for_format(a.format)] forKey:@"ext"];
    [item setObject:[NSNumber numberWithInt:a.format] forKey:@"fmt"];
    [item setObject:[NSNumber numberWithLongLong:(long long)n] forKey:@"size"];
    [item setObject:[NSNumber numberWithLong:a.duration_ms] forKey:@"dur"];
    [item setObject:[NSNumber numberWithInt:a.bitrate_kbps] forKey:@"br"];
    [item setObject:[NSNumber numberWithInt:a.sample_rate] forKey:@"sr"];
    [item setObject:[NSNumber numberWithInt:a.channels] forKey:@"ch"];
    NSString *title = Clean([NSString stringWithUTF8String:t.title], 250);
    if (![title length]) title = Clean([[[item objectForKey:@"name"] lastPathComponent] stringByDeletingPathExtension], 250);
    if (![title length]) title = GL(@"Bez tytułu");
    [item setObject:title forKey:@"title"];
    [item setObject:Clean([NSString stringWithUTF8String:t.artist], 250) forKey:@"artist"];
    [item setObject:Clean([NSString stringWithUTF8String:t.album], 250) forKey:@"album"];
    [item setObject:Clean([NSString stringWithUTF8String:t.albumartist], 250) forKey:@"albumartist"];
    [item setObject:Clean([NSString stringWithUTF8String:t.genre], 120) forKey:@"genre"];
    [item setObject:[NSNumber numberWithInt:t.year] forKey:@"year"];
    [item setObject:[NSNumber numberWithInt:t.track] forKey:@"track"];
    [item setObject:[NSNumber numberWithInt:t.track_total] forKey:@"tracks"];
    [item setObject:[NSNumber numberWithInt:t.disc] forKey:@"disc"];
    [item setObject:[NSNumber numberWithInt:t.disc_total] forKey:@"discs"];
    return nil;
}

static NSString *CleanFileName(NSString *name)
{
    NSString *n = [name lastPathComponent];
    if (![n length]) n = @"plik";
    return n;
}

- (NSDictionary *)addFileAtPath:(NSString *)path displayName:(NSString *)name move:(BOOL)move error:(NSString **)err
{
    NSFileManager *fm = [NSFileManager defaultManager];
    if (![fm fileExistsAtPath:path]) { if (err) *err = GL(@"Nie znaleziono pliku"); return nil; }
    NSMutableDictionary *item = [NSMutableDictionary dictionary];
    NSString *iid = NewId();
    [item setObject:iid forKey:@"id"];
    [item setObject:CleanFileName(name ? name : [path lastPathComponent]) forKey:@"name"];
    [item setObject:@"ready" forKey:@"status"];
    [item setObject:[NSNumber numberWithDouble:[[NSDate date] timeIntervalSince1970]] forKey:@"added"];
    /* tymczasowa nazwa z rozszerzeniem z oryginału; właściwe ustalamy po rozpoznaniu formatu */
    NSString *firstExt = [[path pathExtension] lowercaseString];
    if (![firstExt length]) firstExt = @"bin";
    [item setObject:firstExt forKey:@"ext"];
    NSString *dest = [self pathForItem:item];
    NSError *e = nil;
    BOOL ok = move ? [fm moveItemAtPath:path toPath:dest error:&e] : [fm copyItemAtPath:path toPath:dest error:&e];
    if (!ok) {
        if (move) { e = nil; ok = [fm copyItemAtPath:path toPath:dest error:&e]; if (ok) [fm removeItemAtPath:path error:NULL]; }
        if (!ok) { if (err) *err = GL(@"Nie udało się skopiować pliku do kolejki"); return nil; }
    }
    NSString *problem = [self analyzeItem:item];
    if (problem) {
        [fm removeItemAtPath:dest error:NULL];
        if (err) *err = problem;
        return nil;
    }
    /* poprawne rozszerzenie pliku (np. plik .bin okazał się MP3) */
    NSString *newPath = [self pathForItem:item];
    if (![newPath isEqualToString:dest]) [fm moveItemAtPath:dest toPath:newPath error:NULL];
    [[_root objectForKey:@"queue"] addObject:item];
    [self save];
    [self notify];
    return item;
}

- (NSString *)addDownloadingItemForURL:(NSString *)url name:(NSString *)name
{
    NSMutableDictionary *item = [NSMutableDictionary dictionary];
    NSString *iid = NewId();
    [item setObject:iid forKey:@"id"];
    [item setObject:(name ? name : @"") forKey:@"name"];
    [item setObject:url forKey:@"url"];
    [item setObject:@"downloading" forKey:@"status"];
    [item setObject:[NSNumber numberWithFloat:0.0f] forKey:@"progress"];
    [item setObject:[NSNumber numberWithDouble:[[NSDate date] timeIntervalSince1970]] forKey:@"added"];
    [item setObject:@"" forKey:@"title"];
    [[_root objectForKey:@"queue"] addObject:item];
    [self notify];
    return iid;
}

- (void)finishDownloadId:(NSString *)iid tempPath:(NSString *)tmp suggestedName:(NSString *)name error:(NSString *)err
{
    NSMutableDictionary *item = [self mutableItemWithId:iid];
    NSFileManager *fm = [NSFileManager defaultManager];
    if (!item) { if (tmp) [fm removeItemAtPath:tmp error:NULL]; return; }
    if (err) {
        if (tmp) [fm removeItemAtPath:tmp error:NULL];
        [item setObject:@"error" forKey:@"status"];
        [item setObject:err forKey:@"error"];
        [item removeObjectForKey:@"progress"];
        [self save]; [self notify];
        return;
    }
    if ([name length]) [item setObject:CleanFileName(name) forKey:@"name"];
    NSString *ext = [[[item objectForKey:@"name"] pathExtension] lowercaseString];
    [item setObject:[ext length] ? ext : @"bin" forKey:@"ext"];
    NSString *dest = [self pathForItem:item];
    [fm removeItemAtPath:dest error:NULL];
    if (![fm moveItemAtPath:tmp toPath:dest error:NULL]) {
        [item setObject:@"error" forKey:@"status"];
        [item setObject:GL(@"Nie udało się zapisać pobranego pliku") forKey:@"error"];
        [self save]; [self notify];
        return;
    }
    NSString *problem = [self analyzeItem:item];
    if (problem) {
        [fm removeItemAtPath:dest error:NULL];
        [item setObject:@"error" forKey:@"status"];
        [item setObject:problem forKey:@"error"];
    } else {
        NSString *newPath = [self pathForItem:item];
        if (![newPath isEqualToString:dest]) [fm moveItemAtPath:dest toPath:newPath error:NULL];
        [item setObject:@"ready" forKey:@"status"];
        [item removeObjectForKey:@"error"];
    }
    [item removeObjectForKey:@"progress"];
    [self save];
    [self notify];
}

- (void)updateItemId:(NSString *)iid values:(NSDictionary *)values save:(BOOL)save
{
    NSMutableDictionary *item = [self mutableItemWithId:iid];
    if (!item) return;
    [item addEntriesFromDictionary:values];
    if (save) [self save];
    [self notify];
}

- (void)deleteItemId:(NSString *)iid
{
    NSMutableDictionary *item = [self mutableItemWithId:iid];
    if (!item) return;
    [[NSFileManager defaultManager] removeItemAtPath:[self pathForItem:item] error:NULL];
    [[_root objectForKey:@"queue"] removeObject:item];
    [self save];
    [self notify];
}

- (void)clearQueue
{
    for (NSDictionary *it in [self queue]) [[NSFileManager defaultManager] removeItemAtPath:[self pathForItem:it] error:NULL];
    [[_root objectForKey:@"queue"] removeAllObjects];
    [self save];
    [self notify];
}

/* ---------- zaimportowane ---------- */

- (NSArray *)imported { return [NSArray arrayWithArray:[_root objectForKey:@"imported"]]; }

- (void)addImported:(NSDictionary *)rec
{
    [[_root objectForKey:@"imported"] addObject:rec];
    [self save];
}

- (void)removeImportedPid:(NSString *)pid
{
    NSMutableArray *list = [_root objectForKey:@"imported"];
    NSMutableArray *gone = [NSMutableArray array];
    for (NSDictionary *r in list) if ([[r objectForKey:@"pid"] isEqualToString:pid]) [gone addObject:r];
    [list removeObjectsInArray:gone];
    [self save];
}

@end
