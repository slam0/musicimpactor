#import <Foundation/Foundation.h>

/* Prosta diagnostyka: aplikacja zapisuje w Documents ostatni "krok" oraz treść nieobsłużonego wyjątku.
   Przy następnym uruchomieniu pokazuje to w okienku, żeby można było zrobić zdjęcie ekranu. */

void MICrashInstall(void);
void MICrumb(NSString *step);                 /* zapamiętuje, co aplikacja właśnie robi */
void MICrumbCleanExit(void);                  /* zwykłe wyjście do tła: zaznacza, że wszystko było w porządku */
NSString *MITakeLastRunReport(void);          /* tekst o poprzednim, nieudanym uruchomieniu albo nil; czyści pliki */
