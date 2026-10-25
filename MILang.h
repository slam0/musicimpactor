#import <Foundation/Foundation.h>

/* Języki: polski (podstawowy), angielski, rosyjski.
   W kodzie wszystkie napisy są po polsku, a GL() podmienia je na wybrany język. */

extern NSString *const MIAppearanceDidChangeNotification;   /* zmiana języka albo motywu */

/* Język faktycznie używany: 0 polski, 1 angielski, 2 rosyjski. */
NSInteger MILanguage(void);
/* Ustawienie zapisane w pliku: 0 automatycznie (wg telefonu), 1 polski, 2 English, 3 Русский. */
NSInteger MILanguageSetting(void);
void MISetLanguageSetting(NSInteger setting);
NSString *MILanguageSettingName(NSInteger setting);          /* nazwa w jej własnym języku */
NSLocale *MILocale(void);

/* Tłumaczy polski napis; gdy brak tłumaczenia, zwraca go bez zmian. */
NSString *GL(NSString *pl);
