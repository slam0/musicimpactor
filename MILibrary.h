#import <Foundation/Foundation.h>

/* Cienka nakładka na mi_lib.c: zapis do biblioteki aplikacji Muzyka, kopie zapasowe, restart aplikacji. */

@interface MILibrary : NSObject

+ (NSString *)databasePath;
+ (BOOL)databaseExists;

/* items: elementy kolejki ze statusem "ready". Wywołuj w tle (może chwilę trwać).
   Zwraca: @"report" (NSString), @"imported" (NSNumber - ile się udało), @"fatal" (NSNumber BOOL),
           @"results" (NSArray słowników: @"id", @"ok", @"error", oraz dla udanych @"record" - wpis do listy zaimportowanych). */
+ (NSDictionary *)importItems:(NSArray *)items;

/* Usuwa wcześniej zaimportowany utwór z biblioteki i dysku. */
+ (BOOL)removeRecord:(NSDictionary *)record report:(NSString **)report;

+ (NSString *)diagnose;
+ (BOOL)restoreLatestBackup:(NSString **)report;

/* Zamyka procesy o podanej nazwie; zwraca liczbę zamkniętych. */
+ (int)killProcessesNamed:(NSArray *)names;
+ (int)closeMusicApp;
+ (void)respring;

@end
