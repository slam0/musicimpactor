#import <Foundation/Foundation.h>

/* Pobieranie pliku z bezpośredniego linku do kolejki. Klasy sieciowe (NSURL, NSURLConnection...) są wyszukiwane po
   nazwie w czasie działania - patrz uwaga w README. */

@interface MIDownloads : NSObject

+ (MIDownloads *)shared;

/* Zwraca identyfikator elementu kolejki albo nil (wtedy *error ma opis). */
- (NSString *)startWithAddress:(NSString *)address error:(NSString **)error;
- (void)cancelId:(NSString *)iid;                 /* przerywa i usuwa element z kolejki */
- (void)retryId:(NSString *)iid;                  /* ponawia pobieranie elementu z błędem */
- (NSUInteger)activeCount;

@end
