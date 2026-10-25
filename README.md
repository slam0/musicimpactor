# Music Impactor – piosenki prosto do aplikacji Muzyka (iOS 6)

Aplikacja na iPhone'a z jailbreakiem (iOS 6, armv7). Wkładasz pobrane piosenki (MP3, M4A, WAV), a ona sama dopisuje je do biblioteki
aplikacji **Muzyka** – bez komputera i bez synchronizacji z iTunes.

**Wersja 1.0.0** – nie była jeszcze sprawdzona na prawdziwym telefonie. Najbardziej ryzykowna jest część zapisująca do bazy Muzyki (patrz niżej).

## Instalacja (ważne)

Music Impactor musi być zainstalowany jako **paczka `.deb`** (przez Cydię albo Filzę), a nie jako `.ipa`.
Aplikacja z `.ipa` działa w piaskownicy i nie ma prawa zapisywać w folderze Muzyki (`/var/mobile/Media/iTunes_Control`).
Aplikacja z `.deb` leży w `/Applications` i ma dostęp do plików użytkownika `mobile`.

## Jak dodać piosenki

1. **Link** – zakładka *Kolejka* → **Dodaj → Pobierz z linku**, wklej bezpośredni adres pliku (kończący się np. na `.mp3`). Link do strony z odtwarzaczem nie zadziała.
2. **„Otwórz w…”** – w innej aplikacji (Mail, Dropbox, menedżer plików) wybierz plik audio → *Otwórz w…* → **Music Impactor**.
3. **iTunes** – podłącz telefon, iTunes → telefon → Aplikacje → Udostępnianie plików → Music Impactor → przeciągnij pliki (mogą być w podfolderach). W aplikacji: **Dodaj → Pliki z folderu aplikacji**.

Każdy plik trafia do kolejki. Aplikacja sama rozpoznaje format i czyta tagi (tytuł, wykonawca, album, rok, numer utworu, długość). Dotknij utworu, żeby poprawić tagi;
przycisk *Zastosuj do całej kolejki* kopiuje wykonawcę, album i rok do pozostałych utworów (wygodne przy albumie).
Na górze listy pojawia się zielony przycisk **Dodaj do Muzyki**.

Zakładka *Zaimportowane* to lista utworów dodanych przez aplikację; przesuń palcem po wierszu, żeby usunąć utwór z Muzyki (znika plik i wpisy w bibliotece).

Nie ma pobierania z YouTube ani ze Spotify – to narusza prawa autorskie i regulaminy tych serwisów. Używaj własnych nagrań, zakupionych plików i muzyki na wolnych licencjach.

## Co robi przy zapisie (i jak się zabezpiecza)

Muzyka na iOS 6 trzyma bibliotekę w bazie SQLite `/var/mobile/Media/iTunes_Control/iTunes/MediaLibrary.sqlitedb`, a pliki w `iTunes_Control/Music/F00…F49/ABCD.mp3`.
Dokładnego schematu tej bazy nie znamy na pewno, dlatego zapis jest „adaptacyjny”:

* sprawdza, jakie tabele i kolumny są w bazie, a jako wzór bierze wiersze jednego istniejącego utworu (najlepiej w tym samym formacie) i klonuje je z podmienionymi polami,
* wykonawcę, album, wykonawcę albumu i gatunek znajduje albo tworzy,
* **przed zapisem robi kopię bazy** (`Documents/Kopie/`, zostaje 5 ostatnich), pracuje w jednej transakcji i sprawdza spójność bazy (`PRAGMA quick_check`) – przy błędzie wszystko jest wycofywane,
* gdy baza jest zajęta (otwarta Muzyka), przerywa bez zmian,
* o wszystkim pisze w raporcie (Ustawienia → Ostatni raport), który można skopiować.

Logika zapisu jest sprawdzona na kilku sztucznych bazach (`tests/lib_test.py`: pełny schemat, pusta biblioteka, minimalny schemat, daty od 1970 i od 2001, własne porównywanie napisów, wyzwalacz z nieznaną funkcją, uszkodzona i zablokowana baza).
Prawdziwą bazę z iOS 6 zobaczy dopiero Twój telefon – jeśli coś nie zadziała, wejdź w **Ustawienia → Diagnostyka biblioteki** i **Ostatni raport**, skopiuj oba teksty i prześlij autorowi.

Po dodaniu aplikacja zamyka aplikację Muzyka (można wyłączyć w Ustawieniach), żeby wczytała bibliotekę od nowa. Gdy utworów nadal nie widać: Ustawienia → **Zrestartuj (respring)**, a w ostateczności zrestartuj telefon.
Gdyby coś poszło źle: Ustawienia → **Przywróć kopię biblioteki**.

## Formaty

MP3, M4A (AAC/ALAC) i WAV. FLAC, OGG, AIFF i WMA są rozpoznawane, ale aplikacja Muzyka na iOS 6 ich nie odtwarza – dostaniesz wtedy czytelny komunikat.
Okładek wbudowanych w plik jeszcze nie wczytuje.

## Dane aplikacji

`Documents/musicimpactor.json` (ustawienia, kolejka, lista zaimportowanych), `Documents/Kolejka/` (pliki w kolejce), `Documents/Kopie/` (kopie bazy Muzyki), `Documents/raport.txt` (ostatni raport).
Widać je w iTunes → Aplikacje → Udostępnianie plików. Uszkodzony `musicimpactor.json` jest odkładany jako `musicimpactor.json.uszkodzony`.

## Diagnostyka awarii

Aplikacja zapisuje w `Documents` ostatni wykonywany „krok” (`ostatni-krok.txt`) i treść nieobsłużonego błędu (`blad.txt`). Jeśli poprzednie uruchomienie
zakończyło się awarią, po następnym starcie pokaże okienko „Music Impactor – diagnostyka” z tą informacją – wystarczy zrobić zdjęcie ekranu.

## Budowanie

1. Wgraj całą zawartość tego folderu do nowego publicznego repozytorium **MusicImpactor** na GitHubie (folder `.github` jest ukryty – w Finderze Cmd+Shift+. ; jeśli się nie wgrał, dodaj plik `build.yml` ręcznie w `.github/workflows/`).
2. Settings → Actions → General → Workflow permissions → **Read and write permissions**.
3. Actions → **Zbuduj aplikację** → Run workflow (nie „Re-run all jobs”). Powstaje `.deb`, który trafia do wydania `najnowsza`.
4. Żeby aplikacja była w Cydii: dopisz `slam0/MusicImpactor` do pliku `projekty.txt` w repozytorium `cydia` i wgraj `icon-512.png` jako `site/icons/musicimpactor.png`.

Lokalnie: `./build.sh` (potrzebny Theos). Testy: `gcc -Wall mi_core.c tests/core_test.c -lm -o core_test && ./core_test` oraz `python3 tests/lib_test.py` (potrzebuje `libsqlite3-dev`).
Ikona i ekrany startowe powstają ze skryptu `python3 tools/make_images.py` (Pillow).

## Pliki

| Plik | Rola |
|---|---|
| `mi_core.*` | logika w czystym C: rozpoznawanie formatu, tagi ID3v2/ID3v1/MP4/WAV, długość i bitrate, nazwy plików w folderze Muzyki; testy w `tests/core_test.c` |
| `mi_lib.*` | zapis do `MediaLibrary.sqlitedb` (adaptacyjny, z kopią i transakcją), usuwanie, diagnostyka, przywracanie kopii; testy w `tests/lib_test.py` |
| `mi_libc.c` | własne `memcpy`/`memset`/`strcmp`… (zastępcze pliki SDK z GitHuba ich nie mają) |
| `MIStore.*` | ustawienia, kolejka i lista zaimportowanych (`musicimpactor.json`) |
| `MIDownloads.*` | pobieranie z linku (z postępem, w tle do kilku minut) |
| `MILibrary.*` | nakładka na `mi_lib` + zamykanie aplikacji Muzyka i respring |
| `MIQueueController.*` | kolejka, dodawanie, import |
| `MIEditController.*` | edycja tagów |
| `MIFilesController.*` | pliki z folderu aplikacji (iTunes, „Otwórz w…”) |
| `MIImportedController.*` | zaimportowane utwory, usuwanie z Muzyki |
| `MITextController.*` | raporty do skopiowania |
| `MISettingsController.*` | ustawienia, język, motyw, narzędzia biblioteki |
| `MILang.*` | języki: tabela tłumaczeń `GL(@"polski napis")` (PL/EN/RU) |
| `MITheme.*`, `MIViews.*` | wygląd: motywy i paleta, paski, żelowe przyciski, komórki, ikonki |
| `MICrash.*` | diagnostyka po awarii |

## Uwaga o budowaniu (ważne przy zmianach w kodzie)

Zastępcze pliki SDK z GitHuba przypisują niektóre klasy do złych bibliotek (np. `NSURL`, `NSHTTPURLResponse`, `NSURLConnection`, `NSMutableURLRequest` do CFNetwork),
a na iOS 6 ich tam nie ma. Odwołanie do takiej klasy kończy się błędem dyld „Symbol not found” zaraz po starcie. Dlatego `MIDownloads` i `MIViews` wyszukują je po nazwie
(`NSClassFromString`) i wysyłają do nich komunikaty przez `id`. Nie dzielimy też liczb 64-bitowych ani przez zmienne (brak `___divdi3`/`___divsi3` w zastępczej bibliotece).
Sprawdzenie po zmianach: skompiluj pliki dla `armv7-apple-ios6.0` i obejrzyj `llvm-nm -u` na plikach obiektowych.

## Dodawanie języka albo motywu

* Napisy w kodzie są po polsku i opakowane w `GL(@"…")`; tłumaczenie to jeden wiersz `{ PL, EN, RU }` w `MILang.m`. Brak wiersza = napis zostaje po polsku.
* Motyw to jeden wiersz w tabeli `kThemes` w `MITheme.m`.
