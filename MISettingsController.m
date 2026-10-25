#import "MISettingsController.h"
#import "MIStore.h"
#import "MITheme.h"
#import "MILang.h"
#import "MIViews.h"
#import "MILibrary.h"
#import "MITextController.h"
#import "MIDownloads.h"

/* ptaszek w kolorze motywu (systemowy jest ciemnoniebieski i na grafitowym tle słabo widać) */
static UIImage *MITickImage(void)
{
    CGFloat s = 22.0f;
    UIGraphicsBeginImageContextWithOptions(CGSizeMake(s, s), NO, 0.0f);
    UIBezierPath *p = [UIBezierPath bezierPath];
    [p moveToPoint:CGPointMake(3, 11.5f)];
    [p addLineToPoint:CGPointMake(8.5f, 17)];
    [p addLineToPoint:CGPointMake(19, 5)];
    p.lineWidth = 3.6f;
    p.lineCapStyle = kCGLineCapRound;
    p.lineJoinStyle = kCGLineJoinRound;
    [MIAccentColor() setStroke];
    [p stroke];
    UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return img;
}

@implementation MIChoiceController
{
    UITableView *_table;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.backgroundColor = MIBackgroundColor();
    _table = [[UITableView alloc] initWithFrame:self.view.bounds style:UITableViewStylePlain];
    _table.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _table.dataSource = self;
    _table.delegate = self;
    _table.rowHeight = 52;
    MIStyleTable(_table);
    [self.view addSubview:_table];
}

- (NSInteger)tableView:(UITableView *)t numberOfRowsInSection:(NSInteger)s { return (NSInteger)[self.titles count]; }

- (UITableViewCell *)tableView:(UITableView *)t cellForRowAtIndexPath:(NSIndexPath *)ip
{
    MICell *c = [MICell cellForTable:t identifier:@"choice"];
    c.textLabel.text = [self.titles objectAtIndex:(NSUInteger)ip.row];
    c.detailTextLabel.text = nil;
    c.imageView.image = ip.row < (NSInteger)[self.images count] ? [self.images objectAtIndex:(NSUInteger)ip.row] : nil;
    c.accessoryView = ip.row == self.selected ? [[UIImageView alloc] initWithImage:MITickImage()] : nil;
    return c;
}

- (void)tableView:(UITableView *)t didSelectRowAtIndexPath:(NSIndexPath *)ip
{
    [t deselectRowAtIndexPath:ip animated:YES];
    if (ip.row == self.selected) return;
    self.selected = ip.row;
    [t reloadData];
    if (self.onChoose) self.onChoose(ip.row);
}

@end

/* ---------------------------------------------------------------------- */

@interface MISettingsController ()
{
    UITableView *_table;
    UIView *_busy;
    NSInteger _sheetKind;
}
@end

@implementation MISettingsController

- (id)init
{
    if ((self = [super init])) self.title = GL(@"Ustawienia");
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.backgroundColor = MIBackgroundColor();
    _table = [[UITableView alloc] initWithFrame:self.view.bounds style:UITableViewStylePlain];
    _table.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _table.dataSource = self;
    _table.delegate = self;
    _table.rowHeight = 56;
    MIStyleTable(_table);
    [self.view addSubview:_table];
}

- (void)viewWillAppear:(BOOL)animated { [super viewWillAppear:animated]; [_table reloadData]; }

/* sekcje: 0 Wygląd, 1 Import, 2 Biblioteka, 3 O aplikacji */
- (NSInteger)numberOfSectionsInTableView:(UITableView *)t { return 4; }

- (NSInteger)tableView:(UITableView *)t numberOfRowsInSection:(NSInteger)s
{
    return s == 0 ? 2 : (s == 1 ? 2 : (s == 2 ? 6 : 5));
}

- (CGFloat)tableView:(UITableView *)t heightForHeaderInSection:(NSInteger)s { return 26; }

- (CGFloat)tableView:(UITableView *)t heightForRowAtIndexPath:(NSIndexPath *)ip
{
    if (ip.section == 3) return ip.row >= 2 ? (ip.row == 2 ? 104 : (ip.row == 3 ? 118 : 96)) : 56;
    if (ip.section == 1 || ip.section == 2) return 74;
    return 56;
}

- (UIView *)tableView:(UITableView *)t viewForHeaderInSection:(NSInteger)s
{
    NSArray *n = [NSArray arrayWithObjects:@"Wygląd", @"Import", @"Biblioteka Muzyki", @"O aplikacji", nil];
    return MIHeaderForTitle(GL([n objectAtIndex:(NSUInteger)s]), nil);
}

- (UISwitch *)switchWithTag:(NSInteger)tag on:(BOOL)on
{
    UISwitch *sw = [[UISwitch alloc] init];
    sw.on = on;
    sw.tag = tag;
    [sw addTarget:self action:@selector(switchChanged:) forControlEvents:UIControlEventValueChanged];
    return sw;
}

- (void)switchChanged:(UISwitch *)sw
{
    MIStore *st = [MIStore shared];
    if (sw.tag == 0) st.removeAfterImport = sw.on;
    else if (sw.tag == 1) st.restartMusic = sw.on;
}

- (UITableViewCell *)tableView:(UITableView *)t cellForRowAtIndexPath:(NSIndexPath *)ip
{
    MICell *c = [MICell cellForTable:t identifier:@"s"];
    c.accessoryType = UITableViewCellAccessoryNone;
    c.accessoryView = nil;
    c.imageView.image = nil;
    c.selectionStyle = UITableViewCellSelectionStyleBlue;
    c.textLabel.textColor = MITextColor();
    c.detailTextLabel.numberOfLines = 4;
    MIStore *st = [MIStore shared];
    if (ip.section == 0) {
        c.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
        if (ip.row == 0) {
            c.textLabel.text = GL(@"Język");
            c.detailTextLabel.text = MILanguageSettingName(MILanguageSetting());
        } else {
            c.textLabel.text = GL(@"Motyw");
            c.detailTextLabel.text = MIThemeName(MICurrentTheme());
            c.imageView.image = MIThemeSwatch(MICurrentTheme());
        }
    } else if (ip.section == 1) {
        c.selectionStyle = UITableViewCellSelectionStyleNone;
        if (ip.row == 0) {
            c.textLabel.text = GL(@"Usuń z kolejki po dodaniu");
            c.detailTextLabel.text = GL(@"Plik z kolejki jest już niepotrzebny, bo utwór ma swoją kopię w Muzyce (oszczędza miejsce)");
            c.accessoryView = [self switchWithTag:0 on:st.removeAfterImport];
        } else {
            c.textLabel.text = GL(@"Zamknij Muzykę po dodaniu");
            c.detailTextLabel.text = GL(@"Aplikacja Muzyka wczyta wtedy nową bibliotekę przy następnym otwarciu");
            c.accessoryView = [self switchWithTag:1 on:st.restartMusic];
        }
    } else if (ip.section == 2) {
        switch (ip.row) {
            case 0:
                c.textLabel.text = GL(@"Ostatni raport");
                c.detailTextLabel.text = GL(@"Dokładny zapis ostatniego dodawania do Muzyki - przydatny, gdy coś nie działa");
                c.accessoryType = UITableViewCellAccessoryDisclosureIndicator; break;
            case 1:
                c.textLabel.text = GL(@"Diagnostyka biblioteki");
                c.detailTextLabel.text = GL(@"Opis tabel i kolumn bazy Muzyki do skopiowania i przesłania autorowi");
                c.accessoryType = UITableViewCellAccessoryDisclosureIndicator; break;
            case 2:
                c.textLabel.text = GL(@"Przywróć kopię biblioteki");
                c.textLabel.textColor = MIRGB(MIP()->danger, 1);
                c.detailTextLabel.text = GL(@"Wraca do stanu sprzed ostatniego dodawania (utwory dodane później znikną z Muzyki)"); break;
            case 3:
                c.textLabel.text = GL(@"Zamknij aplikację Muzyka");
                c.detailTextLabel.text = GL(@"Żeby przy następnym otwarciu wczytała bibliotekę od nowa"); break;
            case 4:
                c.textLabel.text = GL(@"Zrestartuj (respring)");
                c.detailTextLabel.text = GL(@"Ponowne uruchomienie ekranu głównego; pomaga, gdy nowych utworów nadal nie widać"); break;
            default:
                c.textLabel.text = GL(@"Folder kopii bazy");
                c.detailTextLabel.text = GL(@"Kopie leżą w iTunes → Aplikacje → Udostępnianie plików → folder Kopie (zostaje 5 ostatnich)");
                c.selectionStyle = UITableViewCellSelectionStyleNone; break;
        }
    } else {
        c.selectionStyle = UITableViewCellSelectionStyleNone;
        switch (ip.row) {
            case 0: c.textLabel.text = GL(@"Wersja"); c.detailTextLabel.text = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"]; break;
            case 1: c.textLabel.text = GL(@"Autor"); c.detailTextLabel.text = @"nazar0vsky-slam0"; break;
            case 2: c.textLabel.text = GL(@"Uwaga"); c.detailTextLabel.text = GL(@"Używaj tylko muzyki, do której masz prawa (własne nagrania, zakupione pliki, utwory na wolnych licencjach). Aplikacja nie pobiera muzyki z YouTube ani ze Spotify."); break;
            case 3: c.textLabel.text = GL(@"Jak to działa"); c.detailTextLabel.text = GL(@"Aplikacja kopiuje plik do folderu Muzyki i dopisuje go do biblioteki (MediaLibrary.sqlitedb). Dlatego musi być zainstalowana jako .deb przez Cydię, a nie jako .ipa. Obsługiwane formaty: MP3, M4A (AAC), WAV."); break;
            default: c.textLabel.text = GL(@"Gdy coś pójdzie nie tak"); c.detailTextLabel.text = GL(@"Skopiuj \"Ostatni raport\" i \"Diagnostykę biblioteki\" i prześlij je autorowi. Kopię biblioteki przywrócisz przyciskiem wyżej."); break;
        }
    }
    return c;
}

- (void)pushChoice:(NSArray *)titles images:(NSArray *)images selected:(NSInteger)sel title:(NSString *)title handler:(void (^)(NSInteger))handler
{
    MIChoiceController *cc = [[MIChoiceController alloc] init];
    cc.title = title;
    cc.titles = titles;
    cc.images = images;
    cc.selected = sel;
    cc.onChoose = handler;
    [self.navigationController pushViewController:cc animated:YES];
}

- (void)showText:(NSString *)title text:(NSString *)text
{
    [MITextController presentTitle:title text:text from:self.tabBarController ? (UIViewController *)self.tabBarController : self];
}

- (void)setBusy:(BOOL)on
{
    if (!on) { [_busy removeFromSuperview]; _busy = nil; return; }
    UIView *host = self.tabBarController.view ? self.tabBarController.view : self.view;
    _busy = [[UIView alloc] initWithFrame:host.bounds];
    _busy.backgroundColor = [UIColor colorWithWhite:0.0f alpha:0.6f];
    _busy.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    UIActivityIndicatorView *sp = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhiteLarge];
    sp.center = CGPointMake(host.bounds.size.width / 2, host.bounds.size.height / 2);
    sp.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin;
    [sp startAnimating];
    [_busy addSubview:sp];
    [host addSubview:_busy];
}

- (void)tableView:(UITableView *)t didSelectRowAtIndexPath:(NSIndexPath *)ip
{
    [t deselectRowAtIndexPath:ip animated:YES];
    if (ip.section == 0 && ip.row == 0) {
        NSMutableArray *names = [NSMutableArray array];
        for (NSInteger i = 0; i < 4; i++) [names addObject:MILanguageSettingName(i)];
        [self pushChoice:names images:nil selected:MILanguageSetting() title:GL(@"Język") handler:^(NSInteger i) { MISetLanguageSetting(i); }];
    } else if (ip.section == 0 && ip.row == 1) {
        NSMutableArray *names = [NSMutableArray array], *imgs = [NSMutableArray array];
        for (NSInteger i = 0; i < MIThemeCount(); i++) { [names addObject:MIThemeName(i)]; [imgs addObject:MIThemeSwatch(i)]; }
        [self pushChoice:names images:imgs selected:MICurrentTheme() title:GL(@"Motyw") handler:^(NSInteger i) { MISetTheme(i); }];
    } else if (ip.section == 2) {
        if (ip.row == 0) {
            NSString *r = [MIStore shared].lastReport;
            [self showText:GL(@"Ostatni raport") text:r ? r : GL(@"Nie ma jeszcze żadnego raportu. Pojawi się po pierwszym dodaniu utworów do Muzyki.")];
        } else if (ip.row == 1) {
            [self setBusy:YES];
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                NSString *d = [MILibrary diagnose];
                dispatch_async(dispatch_get_main_queue(), ^{ [self setBusy:NO]; [self showText:GL(@"Diagnostyka biblioteki") text:d]; });
            });
        } else if (ip.row == 2) {
            _sheetKind = 1;
            UIActionSheet *s = [[UIActionSheet alloc] initWithTitle:GL(@"Przywrócić ostatnią kopię biblioteki Muzyki? Utwory dodane po jej zrobieniu znikną z Muzyki (pliki zostaną na dysku).")
                                                           delegate:self cancelButtonTitle:GL(@"Anuluj") destructiveButtonTitle:GL(@"Przywróć kopię") otherButtonTitles:nil];
            [s showFromTabBar:self.tabBarController.tabBar];
        } else if (ip.row == 3) {
            int n = [MILibrary closeMusicApp];
            MIAlert(nil, n > 0 ? GL(@"Aplikacja Muzyka została zamknięta.") : GL(@"Aplikacja Muzyka nie była uruchomiona."));
        } else if (ip.row == 4) {
            _sheetKind = 2;
            UIActionSheet *s = [[UIActionSheet alloc] initWithTitle:GL(@"Ekran główny zostanie uruchomiony ponownie. To bezpieczne i trwa kilka sekund.")
                                                           delegate:self cancelButtonTitle:GL(@"Anuluj") destructiveButtonTitle:GL(@"Zrestartuj") otherButtonTitles:nil];
            [s showFromTabBar:self.tabBarController.tabBar];
        }
    }
}

- (void)actionSheet:(UIActionSheet *)s clickedButtonAtIndex:(NSInteger)i
{
    if (i != s.destructiveButtonIndex) return;
    if (_sheetKind == 1) {
        [MILibrary closeMusicApp];
        NSString *report = nil;
        BOOL ok = [MILibrary restoreLatestBackup:&report];
        MIAlert(ok ? GL(@"Gotowe") : GL(@"Nie udało się"), report);
    } else if (_sheetKind == 2) {
        [[MIStore shared] save];
        [MILibrary respring];
    }
}

@end
