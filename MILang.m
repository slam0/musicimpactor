#import "MILang.h"
#import "MIStore.h"

NSString *const MIAppearanceDidChangeNotification = @"MIAppearanceDidChangeNotification";

static NSInteger gLang = -1;

static NSInteger MIAutoLanguage(void)
{
    NSArray *prefs = [NSLocale preferredLanguages];
    NSString *first = [prefs count] ? [prefs objectAtIndex:0] : @"pl";
    if ([first hasPrefix:@"pl"]) return 0;
    if ([first hasPrefix:@"ru"]) return 2;
    return 1;
}

NSInteger MILanguageSetting(void) { return [MIStore shared].languageSetting; }

NSInteger MILanguage(void)
{
    if (gLang < 0) {
        NSInteger s = MILanguageSetting();
        gLang = (s >= 1 && s <= 3) ? s - 1 : MIAutoLanguage();
    }
    return gLang;
}

void MISetLanguageSetting(NSInteger setting)
{
    if (setting < 0 || setting > 3) setting = 0;
    [MIStore shared].languageSetting = setting;
    gLang = -1;
    [[NSNotificationCenter defaultCenter] postNotificationName:MIAppearanceDidChangeNotification object:nil];
}

NSString *MILanguageSettingName(NSInteger setting)
{
    switch (setting) {
        case 1: return @"Polski";
        case 2: return @"English";
        case 3: return @"Русский";
        default: return GL(@"Automatycznie");
    }
}

NSLocale *MILocale(void)
{
    static NSLocale *cache[3];
    NSInteger l = MILanguage();
    if (!cache[l]) {
        NSString *ids[3] = { @"pl_PL", @"en_GB", @"ru_RU" };
        cache[l] = [[NSLocale alloc] initWithLocaleIdentifier:ids[l]];
    }
    return cache[l];
}

/* polski | angielski | rosyjski */
static const struct { NSString *pl, *en, *ru; } kStrings[] = {
    { @"(brak wykonawcy i albumu - dotknij, aby uzupełnić)", @"(no artist or album - tap to fill in)", @"(нет исполнителя и альбома - нажмите, чтобы заполнить)" },
    { @"Akcje", @"Actions", @"Действия" },
    { @"Album", @"Album", @"Альбом" },
    { @"Anuluj", @"Cancel", @"Отмена" },
    { @"Anuluj pobieranie", @"Cancel download", @"Отменить загрузку" },
    { @"Aplikacja Muzyka nie była uruchomiona.", @"The Music app was not running.", @"Программа «Музыка» не была запущена." },
    { @"Aplikacja Muzyka wczyta wtedy nową bibliotekę przy następnym otwarciu", @"Music will then load the new library the next time it opens", @"«Музыка» загрузит новую медиатеку при следующем открытии" },
    { @"Aplikacja Muzyka została zamknięta.", @"The Music app was closed.", @"Программа «Музыка» закрыта." },
    { @"Aplikacja kopiuje plik do folderu Muzyki i dopisuje go do biblioteki (MediaLibrary.sqlitedb). Dlatego musi być zainstalowana jako .deb przez Cydię, a nie jako .ipa. Obsługiwane formaty: MP3, M4A (AAC), WAV.", @"The app copies the file into the Music folder and adds it to the library (MediaLibrary.sqlitedb). That is why it must be installed as a .deb through Cydia, not as an .ipa. Supported formats: MP3, M4A (AAC), WAV.", @"Программа копирует файл в папку «Музыки» и добавляет его в медиатеку (MediaLibrary.sqlitedb). Поэтому её нужно ставить как .deb через Cydia, а не как .ipa. Поддерживаются форматы: MP3, M4A (AAC), WAV." },
    { @"Automatycznie", @"Automatic", @"Автоматически" },
    { @"Autor", @"Author", @"Автор" },
    { @"Bez tytułu", @"Untitled", @"Без названия" },
    { @"Biblioteka Muzyki", @"Music library", @"Медиатека «Музыки»" },
    { @"Brak połączenia z internetem", @"No internet connection", @"Нет подключения к интернету" },
    { @"Błękit", @"Blue", @"Синий" },
    { @"Czerwień", @"Red", @"Красный" },
    { @"Diagnostyka biblioteki", @"Library diagnostics", @"Диагностика медиатеки" },
    { @"Dodaj", @"Add", @"Добавить" },
    { @"Dodaj do Muzyki", @"Add to Music", @"Добавить в «Музыку»" },
    { @"Dodaj do Muzyki (%d)", @"Add to Music (%d)", @"Добавить в «Музыку» (%d)" },
    { @"Dodaj piosenki, a aplikacja wrzuci je prosto do Muzyki - bez komputera.\n\n• wklej link do pliku MP3 / M4A / WAV,\n• w innej aplikacji (Mail, Dropbox...) wybierz \"Otwórz w...\" i wskaż Music Impactor,\n• albo wgraj pliki przez iTunes (Aplikacje → Udostępnianie plików).", @"Add songs and the app puts them straight into Music - no computer needed.\n\n• paste a link to an MP3 / M4A / WAV file,\n• in another app (Mail, Dropbox...) choose \"Open in...\" and pick Music Impactor,\n• or copy files over with iTunes (Apps → File Sharing).", @"Добавьте песни, и программа положит их прямо в «Музыку» - без компьютера.\n\n• вставьте ссылку на файл MP3 / M4A / WAV,\n• в другой программе (Почта, Dropbox...) выберите \"Открыть в...\" и укажите Music Impactor,\n• или загрузите файлы через iTunes (Программы → Общие файлы)." },
    { @"Dodaj wszystkie", @"Add all", @"Добавить все" },
    { @"Dodano %d plików do kolejki, %d pominięto:\n", @"Added %d files to the queue, %d skipped:\n", @"Добавлено файлов в очередь: %d, пропущено: %d:\n" },
    { @"Dodano %d z %d utworów.", @"Added %d of %d songs.", @"Добавлено песен: %d из %d." },
    { @"Dodać %d utworów do aplikacji Muzyka? Przed zapisem aplikacja zrobi kopię zapasową biblioteki.", @"Add %d songs to the Music app? The app will back up the library before writing.", @"Добавить песен в «Музыку»: %d? Перед записью программа сделает резервную копию медиатеки." },
    { @"Dokładny zapis ostatniego dodawania do Muzyki - przydatny, gdy coś nie działa", @"A detailed log of the last import into Music - useful when something does not work", @"Подробный журнал последнего добавления в «Музыку» - пригодится, если что-то не работает" },
    { @"Dysków", @"Discs", @"Дисков" },
    { @"Długość i rozmiar", @"Length and size", @"Длительность и размер" },
    { @"Edytuj", @"Edit", @"Изменить" },
    { @"Ekran główny zostanie uruchomiony ponownie. To bezpieczne i trwa kilka sekund.", @"The Home screen will restart. This is safe and takes a few seconds.", @"Домашний экран перезапустится. Это безопасно и занимает несколько секунд." },
    { @"Fiolet", @"Purple", @"Фиолетовый" },
    { @"Folder kopii bazy", @"Library backup folder", @"Папка резервных копий" },
    { @"Format", @"Format", @"Формат" },
    { @"Format %@ nie jest obsługiwany przez Muzykę w iOS 6 (potrzebny MP3, M4A albo WAV)", @"The %@ format is not supported by Music on iOS 6 (MP3, M4A or WAV needed)", @"Формат %@ не поддерживается «Музыкой» в iOS 6 (нужен MP3, M4A или WAV)" },
    { @"Gatunek", @"Genre", @"Жанр" },
    { @"Gdy coś pójdzie nie tak", @"If something goes wrong", @"Если что-то пойдёт не так" },
    { @"Gotowe", @"Done", @"Готово" },
    { @"Grafit", @"Graphite", @"Графит" },
    { @"Import", @"Import", @"Добавление" },
    { @"Jak to działa", @"How it works", @"Как это работает" },
    { @"Język", @"Language", @"Язык" },
    { @"Kolejka", @"Queue", @"Очередь" },
    { @"Kolejka jest pusta", @"The queue is empty", @"Очередь пуста" },
    { @"Kopie leżą w iTunes → Aplikacje → Udostępnianie plików → folder Kopie (zostaje 5 ostatnich)", @"Backups are in iTunes → Apps → File Sharing → the Kopie folder (the last 5 are kept)", @"Копии лежат в iTunes → Программы → Общие файлы → папка Kopie (хранятся 5 последних)" },
    { @"Kopiuj", @"Copy", @"Копировать" },
    { @"Motyw", @"Theme", @"Тема" },
    { @"Nazwa pliku", @"File name", @"Имя файла" },
    { @"Nie ma jeszcze żadnego raportu. Pojawi się po pierwszym dodaniu utworów do Muzyki.", @"There is no report yet. It will appear after the first time you add songs to Music.", @"Отчёта пока нет. Он появится после первого добавления песен в «Музыку»." },
    { @"Nie ma pliku /var/mobile/Media/iTunes_Control/iTunes/MediaLibrary.sqlitedb. Ta aplikacja działa tylko na iPhonie z iOS 6 i musi być zainstalowana jako paczka .deb (przez Cydię), nie jako .ipa.", @"The file /var/mobile/Media/iTunes_Control/iTunes/MediaLibrary.sqlitedb does not exist. This app only works on an iPhone with iOS 6 and must be installed as a .deb package (through Cydia), not as an .ipa.", @"Файла /var/mobile/Media/iTunes_Control/iTunes/MediaLibrary.sqlitedb нет. Программа работает только на iPhone с iOS 6 и должна быть установлена как пакет .deb (через Cydia), а не как .ipa." },
    { @"Nie ma tu żadnych plików.\n\nPodłącz telefon do komputera, otwórz iTunes → telefon → Aplikacje → Udostępnianie plików → Music Impactor i przeciągnij tam pliki MP3, M4A albo WAV (mogą być w podfolderach). Potem wróć tutaj.", @"There are no files here.\n\nConnect the phone to a computer, open iTunes → your phone → Apps → File Sharing → Music Impactor and drag MP3, M4A or WAV files there (subfolders are fine). Then come back here.", @"Здесь нет файлов.\n\nПодключите телефон к компьютеру, откройте iTunes → телефон → Программы → Общие файлы → Music Impactor и перетащите туда файлы MP3, M4A или WAV (можно в подпапках). Потом вернитесь сюда." },
    { @"Nie mogę utworzyć pliku (brak miejsca?)", @"Cannot create the file (no space?)", @"Не удалось создать файл (нет места?)" },
    { @"Nie rozpoznaję tego formatu (potrzebny MP3, M4A albo WAV)", @"This format is not recognised (MP3, M4A or WAV needed)", @"Формат не распознан (нужен MP3, M4A или WAV)" },
    { @"Nie udało się", @"Failed", @"Не удалось" },
    { @"Nie udało się pobrać pliku", @"Could not download the file", @"Не удалось скачать файл" },
    { @"Nie udało się połączyć bezpiecznie (HTTPS). Na iOS 6 część stron nie działa - spróbuj linku zaczynającego się od http://", @"Could not connect securely (HTTPS). Some sites do not work on iOS 6 - try a link starting with http://", @"Не удалось установить защищённое соединение (HTTPS). В iOS 6 часть сайтов не работает - попробуйте ссылку, начинающуюся с http://" },
    { @"Nie udało się skopiować pliku do kolejki", @"Could not copy the file to the queue", @"Не удалось скопировать файл в очередь" },
    { @"Nie udało się usunąć", @"Could not delete", @"Не удалось удалить" },
    { @"Nie udało się utworzyć połączenia", @"Could not create the connection", @"Не удалось создать соединение" },
    { @"Nie udało się zapisać pliku (brak miejsca?)", @"Could not save the file (no space?)", @"Не удалось сохранить файл (нет места?)" },
    { @"Nie udało się zapisać pobranego pliku", @"Could not save the downloaded file", @"Не удалось сохранить скачанный файл" },
    { @"Nie znaleziono biblioteki Muzyki", @"Music library not found", @"Медиатека «Музыки» не найдена" },
    { @"Nie znaleziono pliku", @"File not found", @"Файл не найден" },
    { @"Nie znaleziono serwera (sprawdź adres)", @"Server not found (check the address)", @"Сервер не найден (проверьте адрес)" },
    { @"Nr dysku", @"Disc no.", @"Диск №" },
    { @"Nr utworu", @"Track no.", @"Трек №" },
    { @"O aplikacji", @"About", @"О программе" },
    { @"Opis tabel i kolumn bazy Muzyki do skopiowania i przesłania autorowi", @"A description of the Music database tables and columns, to copy and send to the author", @"Описание таблиц и столбцов базы «Музыки», чтобы скопировать и отправить автору" },
    { @"Ostatni raport", @"Last report", @"Последний отчёт" },
    { @"Plik", @"File", @"Файл" },
    { @"Plik jest pusty albo nie da się go odczytać", @"The file is empty or cannot be read", @"Файл пуст или не читается" },
    { @"Plik z kolejki jest już niepotrzebny, bo utwór ma swoją kopię w Muzyce (oszczędza miejsce)", @"The queue file is no longer needed because the song has its own copy in Music (saves space)", @"Файл из очереди больше не нужен, ведь у песни есть своя копия в «Музыке» (экономит место)" },
    { @"Plik zostanie skasowany z aplikacji", @"The file will be deleted from the app", @"Файл будет удалён из программы" },
    { @"Pliki z folderu", @"Files from folder", @"Файлы из папки" },
    { @"Pliki z folderu aplikacji", @"Files from the app folder", @"Файлы из папки программы" },
    { @"Pobieranie", @"Downloading", @"Загрузка" },
    { @"Pobieranie zostało przerwane", @"The download was interrupted", @"Загрузка была прервана" },
    { @"Pobierz", @"Download", @"Скачать" },
    { @"Pobierz z linku", @"Download from a link", @"Скачать по ссылке" },
    { @"Pokaż raport", @"Show report", @"Показать отчёт" },
    { @"Ponowne uruchomienie ekranu głównego; pomaga, gdy nowych utworów nadal nie widać", @"Restarts the Home screen; helps when new songs are still not visible", @"Перезапуск домашнего экрана; помогает, если новых песен всё ещё не видно" },
    { @"Przywrócić ostatnią kopię biblioteki Muzyki? Utwory dodane po jej zrobieniu znikną z Muzyki (pliki zostaną na dysku).", @"Restore the latest Music library backup? Songs added after it was made will disappear from Music (the files stay on disk).", @"Восстановить последнюю копию медиатеки «Музыки»? Песни, добавленные после неё, исчезнут из «Музыки» (файлы останутся на диске)." },
    { @"Przywróć kopię", @"Restore backup", @"Восстановить копию" },
    { @"Przywróć kopię biblioteki", @"Restore library backup", @"Восстановить копию медиатеки" },
    { @"Raport", @"Report", @"Отчёт" },
    { @"Rok", @"Year", @"Год" },
    { @"Serwer nie odpowiada (przekroczono czas)", @"The server is not responding (timed out)", @"Сервер не отвечает (истекло время)" },
    { @"Serwer nie przysłał żadnych danych", @"The server sent no data", @"Сервер не прислал данных" },
    { @"Serwer odpowiedział błędem %d", @"The server answered with error %d", @"Сервер ответил ошибкой %d" },
    { @"Skopiowano do schowka", @"Copied to the clipboard", @"Скопировано в буфер обмена" },
    { @"Skopiuj \"Ostatni raport\" i \"Diagnostykę biblioteki\" i prześlij je autorowi. Kopię biblioteki przywrócisz przyciskiem wyżej.", @"Copy the \"Last report\" and the \"Library diagnostics\" and send them to the author. You can restore the library backup with the button above.", @"Скопируйте \"Последний отчёт\" и \"Диагностику медиатеки\" и отправьте автору. Копию медиатеки можно восстановить кнопкой выше." },
    { @"Spróbuj ponownie", @"Try again", @"Повторить" },
    { @"Srebro", @"Silver", @"Серебро" },
    { @"Tagi", @"Tags", @"Теги" },
    { @"To nie jest plik audio, tylko strona internetowa (sprawdź link)", @"This is not an audio file but a web page (check the link)", @"Это не аудиофайл, а веб-страница (проверьте ссылку)" },
    { @"To nie wygląda na adres internetowy (http:// albo https://)", @"This does not look like a web address (http:// or https://)", @"Это не похоже на веб-адрес (http:// или https://)" },
    { @"Tu pojawią się utwory dodane do Muzyki przez tę aplikację.\n\nStąd możesz je też usunąć z biblioteki (przesuń palcem po wierszu albo dotknij Edytuj).", @"Songs added to Music by this app will appear here.\n\nYou can also remove them from the library here (swipe a row or tap Edit).", @"Здесь появятся песни, добавленные в «Музыку» этой программой.\n\nОтсюда их можно и удалить из медиатеки (смахните строку или нажмите «Изменить»)." },
    { @"Tytuł", @"Title", @"Название" },
    { @"Ustawienia", @"Settings", @"Настройки" },
    { @"Usunąć ten utwór z Muzyki? Plik i wpisy w bibliotece zostaną skasowane.", @"Remove this song from Music? The file and the library entries will be deleted.", @"Удалить эту песню из «Музыки»? Файл и записи в медиатеке будут стёрты." },
    { @"Usunąć ten utwór z kolejki?", @"Remove this song from the queue?", @"Удалить эту песню из очереди?" },
    { @"Usuń", @"Delete", @"Удалить" },
    { @"Usuń z Muzyki", @"Remove from Music", @"Удалить из «Музыки»" },
    { @"Usuń z kolejki", @"Remove from queue", @"Удалить из очереди" },
    { @"Usuń z kolejki po dodaniu", @"Remove from queue after adding", @"Удалять из очереди после добавления" },
    { @"Utworów", @"Tracks", @"Треков" },
    { @"Uwaga", @"Note", @"Важно" },
    { @"Używaj tylko muzyki, do której masz prawa (własne nagrania, zakupione pliki, utwory na wolnych licencjach). Aplikacja nie pobiera muzyki z YouTube ani ze Spotify.", @"Use only music you have the rights to (your own recordings, purchased files, freely licensed songs). The app does not download music from YouTube or Spotify.", @"Используйте только музыку, на которую у вас есть права (собственные записи, купленные файлы, песни со свободными лицензиями). Программа не скачивает музыку с YouTube и Spotify." },
    { @"Wersja", @"Version", @"Версия" },
    { @"Wklej bezpośredni link do pliku MP3, M4A albo WAV.", @"Paste a direct link to an MP3, M4A or WAV file.", @"Вставьте прямую ссылку на файл MP3, M4A или WAV." },
    { @"Wraca do stanu sprzed ostatniego dodawania (utwory dodane później znikną z Muzyki)", @"Goes back to the state before the last import (songs added later will disappear from Music)", @"Возвращает состояние до последнего добавления (песни, добавленные позже, исчезнут из «Музыки»)" },
    { @"Wygląd", @"Appearance", @"Оформление" },
    { @"Wyk. albumu", @"Album artist", @"Исп. альбома" },
    { @"Wykonawca", @"Artist", @"Исполнитель" },
    { @"Wykonawca, album, wykonawca albumu, rok i gatunek zostaną skopiowane do pozostałych utworów", @"Artist, album, album artist, year and genre will be copied to the other songs", @"Исполнитель, альбом, исполнитель альбома, год и жанр будут скопированы в остальные песни" },
    { @"Zaimportowane", @"Imported", @"Добавленные" },
    { @"Zamknij", @"Close", @"Закрыть" },
    { @"Zamknij Muzykę po dodaniu", @"Close Music after adding", @"Закрывать «Музыку» после добавления" },
    { @"Zamknij aplikację Muzyka", @"Close the Music app", @"Закрыть программу «Музыка»" },
    { @"Zapisuję w bibliotece Muzyki...\nNie zamykaj aplikacji.", @"Writing to the Music library...\nDo not close the app.", @"Запись в медиатеку «Музыки»...\nНе закрывайте программу." },
    { @"Zastosuj do całej kolejki", @"Apply to the whole queue", @"Применить ко всей очереди" },
    { @"Zieleń", @"Green", @"Зелёный" },
    { @"Zmieniono %d utworów w kolejce", @"Changed %d songs in the queue", @"Изменено песен в очереди: %d" },
    { @"Zrestartuj", @"Restart", @"Перезапустить" },
    { @"Zrestartuj (respring)", @"Restart (respring)", @"Перезапуск (respring)" },
    { @"\n(+ %d innych błędów)", @"\n(+ %d more errors)", @"\n(+ ещё ошибок: %d)" },
    { @"\n(+ %d innych)", @"\n(+ %d more)", @"\n(+ ещё: %d)" },
    { @"\n\nOtwórz aplikację Muzyka. Jeśli utworów nie widać, uruchom ją ponownie albo użyj \"Zrestartuj (respring)\" w Ustawieniach.", @"\n\nOpen the Music app. If the songs are not visible, restart it or use \"Restart (respring)\" in Settings.", @"\n\nОткройте программу «Музыка». Если песен не видно, перезапустите её или используйте \"Перезапуск (respring)\" в настройках." },
    { @"nie ustawiono", @"not set", @"не задано" },
    { @"Żeby przy następnym otwarciu wczytała bibliotekę od nowa", @"So that it loads the library afresh the next time it opens", @"Чтобы при следующем открытии она заново загрузила медиатеку" },
};

NSString *GL(NSString *pl)
{
    NSInteger l = MILanguage();
    if (l == 0 || !pl) return pl;
    static NSDictionary *tables[2];
    if (!tables[l - 1]) {
        NSMutableDictionary *d = [NSMutableDictionary dictionary];
        for (size_t i = 0; i < sizeof(kStrings) / sizeof(kStrings[0]); i++) {
            [d setObject:(l == 1 ? kStrings[i].en : kStrings[i].ru) forKey:kStrings[i].pl];
        }
        tables[l - 1] = d;
    }
    NSString *t = [tables[l - 1] objectForKey:pl];
    return t ? t : pl;
}
