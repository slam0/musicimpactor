#import "MILibrary.h"
#import "MIStore.h"
#import "mi_lib.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define kReportCap 98304

@implementation MILibrary

+ (NSString *)databasePath
{
    return @"/var/mobile/Media/iTunes_Control/iTunes/MediaLibrary.sqlitedb";
}

+ (BOOL)databaseExists
{
    return [[NSFileManager defaultManager] fileExistsAtPath:[self databasePath]];
}

static NSString *Str(const char *s) { NSString *r = s ? [NSString stringWithUTF8String:s] : nil; return r ? r : @""; }

+ (NSDictionary *)importItems:(NSArray *)items
{
    NSUInteger n = [items count];
    MIStore *store = [MIStore shared];
    mi_lib_track *tracks = (mi_lib_track *)calloc(n ? n : 1, sizeof(mi_lib_track));
    mi_lib_result *res = (mi_lib_result *)calloc(n ? n : 1, sizeof(mi_lib_result));
    char *report = (char *)malloc(kReportCap);
    NSMutableArray *results = [NSMutableArray array];
    NSMutableArray *paths = [NSMutableArray array];       /* trzymamy napisy, żeby wskaźniki UTF8String były ważne */
    if (!tracks || !res || !report) {
        free(tracks); free(res); free(report);
        return [NSDictionary dictionaryWithObjectsAndKeys:@"Brak pamięci", @"report", [NSNumber numberWithInt:0], @"imported", [NSNumber numberWithBool:YES], @"fatal", results, @"results", nil];
    }
    for (NSUInteger i = 0; i < n; i++) {
        NSDictionary *it = [items objectAtIndex:i];
        NSString *path = [store pathForItem:it];
        [paths addObject:path];
        mi_lib_track *t = &tracks[i];
        t->src_path = [path fileSystemRepresentation];
        t->title = [[it objectForKey:@"title"] UTF8String];
        t->artist = [[it objectForKey:@"artist"] UTF8String];
        t->album = [[it objectForKey:@"album"] UTF8String];
        t->albumartist = [[it objectForKey:@"albumartist"] UTF8String];
        t->genre = [[it objectForKey:@"genre"] UTF8String];
        t->year = [[it objectForKey:@"year"] intValue];
        t->track = [[it objectForKey:@"track"] intValue];
        t->track_total = [[it objectForKey:@"tracks"] intValue];
        t->disc = [[it objectForKey:@"disc"] intValue];
        t->disc_total = [[it objectForKey:@"discs"] intValue];
        t->format = [[it objectForKey:@"fmt"] intValue];
        t->duration_ms = [[it objectForKey:@"dur"] longValue];
        t->bitrate_kbps = [[it objectForKey:@"br"] intValue];
        t->sample_rate = [[it objectForKey:@"sr"] intValue];
        t->channels = [[it objectForKey:@"ch"] intValue];
    }
    int ret = mi_lib_import([[self databasePath] fileSystemRepresentation], [[store backupDir] fileSystemRepresentation],
                            tracks, (int)n, res, report, kReportCap);
    NSString *reportText = Str(report);
    int imported = 0;
    for (NSUInteger i = 0; i < n; i++) {
        NSDictionary *it = [items objectAtIndex:i];
        NSMutableDictionary *r = [NSMutableDictionary dictionary];
        [r setObject:[it objectForKey:@"id"] forKey:@"id"];
        [r setObject:[NSNumber numberWithBool:res[i].ok != 0] forKey:@"ok"];
        if (res[i].ok) {
            imported++;
            NSMutableArray *ents = [NSMutableArray array];
            for (int k = 0; k < res[i].ncreated; k++) {
                [ents addObject:[NSDictionary dictionaryWithObjectsAndKeys:
                    Str(res[i].created[k].table), @"table", Str(res[i].created[k].keycol), @"keycol", Str(res[i].created[k].refcol), @"refcol",
                    [NSString stringWithFormat:@"%lld", res[i].created[k].key], @"key", nil]];
            }
            NSDictionary *rec = [NSDictionary dictionaryWithObjectsAndKeys:
                [NSString stringWithFormat:@"%lld", res[i].pid], @"pid", Str(res[i].rel), @"rel",
                [it objectForKey:@"title"] ? [it objectForKey:@"title"] : @"", @"title",
                [it objectForKey:@"artist"] ? [it objectForKey:@"artist"] : @"", @"artist",
                [it objectForKey:@"album"] ? [it objectForKey:@"album"] : @"", @"album",
                [it objectForKey:@"size"] ? [it objectForKey:@"size"] : [NSNumber numberWithInt:0], @"size",
                [NSNumber numberWithDouble:[[NSDate date] timeIntervalSince1970]], @"added",
                ents, @"ents", nil];
            [r setObject:rec forKey:@"record"];
        } else {
            NSString *e = Str(res[i].error);
            if (![e length]) e = ret < 0 ? @"Nie udało się zapisać w bazie (patrz raport)" : @"Nieznany błąd";
            [r setObject:e forKey:@"error"];
        }
        [results addObject:r];
    }
    free(tracks); free(res); free(report);
    (void)paths;
    return [NSDictionary dictionaryWithObjectsAndKeys:reportText, @"report", [NSNumber numberWithInt:imported], @"imported",
            [NSNumber numberWithBool:ret < 0], @"fatal", results, @"results", nil];
}

+ (BOOL)removeRecord:(NSDictionary *)record report:(NSString **)reportOut
{
    NSArray *ents = [record objectForKey:@"ents"];
    mi_lib_entity e[5];
    int ne = 0;
    memset(e, 0, sizeof e);
    for (NSDictionary *d in ents) {
        if (ne >= 5) break;
        strncpy(e[ne].table, [[d objectForKey:@"table"] UTF8String], sizeof e[ne].table - 1);
        strncpy(e[ne].keycol, [[d objectForKey:@"keycol"] UTF8String], sizeof e[ne].keycol - 1);
        strncpy(e[ne].refcol, [[d objectForKey:@"refcol"] UTF8String], sizeof e[ne].refcol - 1);
        e[ne].key = [[d objectForKey:@"key"] longLongValue];
        ne++;
    }
    char *report = (char *)malloc(8192);
    if (!report) return NO;
    int ok = mi_lib_remove([[self databasePath] fileSystemRepresentation], [[record objectForKey:@"pid"] longLongValue],
                           [[record objectForKey:@"rel"] UTF8String], e, ne, report, 8192);
    if (reportOut) *reportOut = Str(report);
    free(report);
    return ok != 0;
}

+ (NSString *)diagnose
{
    char *out = (char *)malloc(kReportCap);
    if (!out) return @"Brak pamięci";
    mi_lib_diagnose([[self databasePath] fileSystemRepresentation], out, kReportCap);
    NSString *s = Str(out);
    free(out);
    return s;
}

+ (BOOL)restoreLatestBackup:(NSString **)reportOut
{
    char *out = (char *)malloc(8192);
    if (!out) return NO;
    int ok = mi_lib_restore_backup([[self databasePath] fileSystemRepresentation], [[[MIStore shared] backupDir] fileSystemRepresentation], out, 8192);
    if (reportOut) *reportOut = Str(out);
    free(out);
    return ok != 0;
}

/* Lista procesów przez sysctl; bez dzielenia (iOS 6 + zastępczy SDK) - przechodzimy po strukturach co sizeof. */
+ (int)killProcessesNamed:(NSArray *)names
{
    int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
    size_t size = 0;
    int killed = 0;
    if (sysctl(mib, 3, NULL, &size, NULL, 0) != 0 || size == 0) return 0;
    size += 8192;
    char *buf = (char *)malloc(size);
    if (!buf) return 0;
    if (sysctl(mib, 3, buf, &size, NULL, 0) == 0) {
        size_t off = 0;
        pid_t me = getpid();
        while (off + sizeof(struct kinfo_proc) <= size) {
            struct kinfo_proc *p = (struct kinfo_proc *)(void *)(buf + off);
            off += sizeof(struct kinfo_proc);
            const char *comm = p->kp_proc.p_comm;
            pid_t pid = p->kp_proc.p_pid;
            if (pid <= 1 || pid == me) continue;
            for (NSString *nm in names) {
                const char *want = [nm UTF8String];
                size_t wl = strlen(want);
                if (wl > 16) wl = 16;                    /* p_comm ma najwyżej 16 znaków */
                if (wl > 0 && strncmp(comm, want, wl) == 0 && (comm[wl] == 0 || wl == 16)) {
                    if (kill(pid, SIGKILL) == 0) killed++;
                    break;
                }
            }
        }
    }
    free(buf);
    return killed;
}

+ (int)closeMusicApp
{
    return [self killProcessesNamed:[NSArray arrayWithObjects:@"MobileMusicPlayer", @"Music", @"iPod", nil]];
}

+ (void)respring
{
    [self killProcessesNamed:[NSArray arrayWithObject:@"SpringBoard"]];
}

@end
