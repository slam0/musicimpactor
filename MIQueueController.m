#import "MIQueueController.h"
#import "MIEditController.h"
#import "MIFilesController.h"
#import "MITextController.h"
#import "MIStore.h"
#import "MIDownloads.h"
#import "MILibrary.h"
#import "MITheme.h"
#import "MILang.h"
#import "MIViews.h"

#define kAlertLink 1
#define kAlertImport 2
#define kAlertResult 3
#define kSheetAdd 1
#define kSheetItem 2

@interface MIQueueController ()
{
    UITableView *_table;
    UIView *_empty;
    UIView *_busy;
    BOOL _reloadScheduled, _pendingReloadWhileEditing;
    NSString *_sheetItemId;
    NSString *_lastReport;
    NSArray *_sheetItemActions;
}
@end

@implementation MIQueueController

- (id)init
{
    if ((self = [super init])) {
        self.title = GL(@"Kolejka");
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(queueChanged:) name:MIQueueDidChangeNotification object:nil];
    }
    return self;
}

- (void)dealloc { [[NSNotificationCenter defaultCenter] removeObserver:self]; }

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.backgroundColor = MIBackgroundColor();
    _table = [[UITableView alloc] initWithFrame:self.view.bounds style:UITableViewStylePlain];
    _table.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _table.dataSource = self;
    _table.delegate = self;
    _table.rowHeight = 68;
    _table.allowsSelectionDuringEditing = NO;
    MIStyleTable(_table);
    [self.view addSubview:_table];
    self.navigationItem.rightBarButtonItem = MIBarButton(GL(@"Dodaj"), self, @selector(addTapped));
    [self buildEmptyView];
    [self refresh];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [self refresh];
}

/* ---------- widok pustej kolejki ---------- */

- (void)buildEmptyView
{
    CGRect b = self.view.bounds;
    _empty = [[UIView alloc] initWithFrame:b];
    _empty.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _empty.backgroundColor = [UIColor clearColor];

    UILabel *title = [[UILabel alloc] initWithFrame:CGRectMake(24, 36, b.size.width - 48, 28)];
    title.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    title.backgroundColor = [UIColor clearColor];
    title.textAlignment = NSTextAlignmentCenter;
    title.font = [UIFont boldSystemFontOfSize:21.0f];
    title.textColor = MITextColor();
    title.text = GL(@"Kolejka jest pusta");
    MIStyleShadow(title);
    [_empty addSubview:title];

    UILabel *hint = [[UILabel alloc] initWithFrame:CGRectMake(24, 72, b.size.width - 48, 190)];
    hint.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    hint.backgroundColor = [UIColor clearColor];
    hint.textAlignment = NSTextAlignmentCenter;
    hint.numberOfLines = 0;
    hint.font = [UIFont systemFontOfSize:15.0f];
    hint.textColor = MISubtleColor();
    hint.text = GL(@"Dodaj piosenki, a aplikacja wrzuci je prosto do Muzyki - bez komputera.\n\n• wklej link do pliku MP3 / M4A / WAV,\n• w innej aplikacji (Mail, Dropbox...) wybierz \"Otwórz w...\" i wskaż Music Impactor,\n• albo wgraj pliki przez iTunes (Aplikacje → Udostępnianie plików).");
    MIStyleShadow(hint);
    [_empty addSubview:hint];

    UIButton *link = MIGelButton(GL(@"Pobierz z linku"), 0, CGRectMake((b.size.width - 220) / 2, 276, 220, 46));
    link.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin;
    [link addTarget:self action:@selector(askLink) forControlEvents:UIControlEventTouchUpInside];
    [_empty addSubview:link];

    UIButton *files = MIGelButton(GL(@"Pliki z folderu aplikacji"), 1, CGRectMake((b.size.width - 220) / 2, 332, 220, 46));
    files.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin;
    [files addTarget:self action:@selector(showFiles) forControlEvents:UIControlEventTouchUpInside];
    [_empty addSubview:files];
    [self.view addSubview:_empty];
}

/* ---------- odświeżanie ---------- */

- (void)queueChanged:(NSNotification *)n { [self reloadSoon]; }

- (void)reloadSoon
{
    if (_reloadScheduled) return;
    _reloadScheduled = YES;
    [self performSelector:@selector(refresh) withObject:nil afterDelay:0.12];
}

- (UIView *)headerView
{
    NSUInteger ready = [[MIStore shared] readyCount];
    if (ready == 0) return nil;
    CGFloat w = _table.bounds.size.width;
    UIView *h = [[UIView alloc] initWithFrame:CGRectMake(0, 0, w, 66)];
    h.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    NSString *t = [NSString stringWithFormat:GL(@"Dodaj do Muzyki (%d)"), (int)ready];
    UIButton *b = MIGelButton(t, 3, CGRectMake(14, 10, w - 28, 46));
    b.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    [b addTarget:self action:@selector(importTapped) forControlEvents:UIControlEventTouchUpInside];
    [h addSubview:b];
    return h;
}

- (void)refresh
{
    _reloadScheduled = NO;
    if (_table.editing) { _pendingReloadWhileEditing = YES; return; }
    NSUInteger n = [[[MIStore shared] queue] count];
    _empty.hidden = n > 0;
    _table.hidden = n == 0;
    if (n == 0) self.navigationItem.leftBarButtonItem = nil;
    else self.navigationItem.leftBarButtonItem = MIBarButton(_table.editing ? GL(@"Gotowe") : GL(@"Edytuj"), self, @selector(editTapped));
    _table.tableHeaderView = [self headerView];
    [_table reloadData];
    [self updateBadge];
}

- (void)updateBadge
{
    NSUInteger ready = [[MIStore shared] readyCount];
    self.navigationController.tabBarItem.badgeValue = ready > 0 ? [NSString stringWithFormat:@"%d", (int)ready] : nil;
}

- (void)editTapped
{
    [_table setEditing:!_table.editing animated:YES];
    self.navigationItem.leftBarButtonItem = MIBarButton(_table.editing ? GL(@"Gotowe") : GL(@"Edytuj"), self, @selector(editTapped));
    if (!_table.editing && _pendingReloadWhileEditing) { _pendingReloadWhileEditing = NO; [self refresh]; }
}

/* ---------- tabela ---------- */

- (NSInteger)tableView:(UITableView *)t numberOfRowsInSection:(NSInteger)s { return (NSInteger)[[[MIStore shared] queue] count]; }

static NSString *FormatLine(NSDictionary *it)
{
    NSMutableArray *p = [NSMutableArray array];
    NSString *ext = [[it objectForKey:@"ext"] uppercaseString];
    if ([ext length]) [p addObject:ext];
    long dur = [[it objectForKey:@"dur"] longValue];
    if (dur > 0) [p addObject:MIDurationString(dur)];
    double size = [[it objectForKey:@"size"] doubleValue];
    if (size > 0) [p addObject:MIBytesString(size)];
    return [p componentsJoinedByString:@" · "];
}

- (UITableViewCell *)tableView:(UITableView *)t cellForRowAtIndexPath:(NSIndexPath *)ip
{
    MICell *c = [MICell cellForTable:t identifier:@"q"];
    NSArray *q = [[MIStore shared] queue];
    c.accessoryView = nil;
    c.accessoryType = UITableViewCellAccessoryNone;
    c.detailTextLabel.numberOfLines = 2;
    c.detailTextLabel.textColor = MISubtleColor();
    if (ip.row >= (NSInteger)[q count]) return c;
    NSDictionary *it = [q objectAtIndex:(NSUInteger)ip.row];
    NSString *status = [it objectForKey:@"status"];
    if ([status isEqualToString:@"downloading"]) {
        NSString *nm = [it objectForKey:@"name"];
        c.textLabel.text = [nm length] ? nm : [it objectForKey:@"url"];
        float p = [[it objectForKey:@"progress"] floatValue];
        double rx = [[it objectForKey:@"received"] doubleValue];
        double ex = [[it objectForKey:@"expectedBytes"] doubleValue];
        if (p >= 0 && ex > 0) c.detailTextLabel.text = [NSString stringWithFormat:@"%@ %d%% (%@ / %@)", GL(@"Pobieranie"), (int)(p * 100.0f + 0.5f), MIBytesString(rx), MIBytesString(ex)];
        else c.detailTextLabel.text = [NSString stringWithFormat:@"%@ %@", GL(@"Pobieranie"), MIBytesString(rx)];
        UIActivityIndicatorView *sp = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:MIP()->light ? UIActivityIndicatorViewStyleGray : UIActivityIndicatorViewStyleWhite];
        [sp startAnimating];
        c.accessoryView = sp;
    } else if ([status isEqualToString:@"error"]) {
        NSString *nm = [it objectForKey:@"name"];
        c.textLabel.text = [nm length] ? nm : (NSString *)[it objectForKey:@"url"];
        c.detailTextLabel.text = GL([it objectForKey:@"error"]);
        c.detailTextLabel.textColor = MIErrorColor();
    } else {
        c.textLabel.text = [it objectForKey:@"title"];
        NSString *artist = [it objectForKey:@"artist"], *album = [it objectForKey:@"album"];
        NSMutableArray *who = [NSMutableArray array];
        if ([artist length]) [who addObject:artist];
        if ([album length]) [who addObject:album];
        NSString *l1 = [who count] ? [who componentsJoinedByString:@" — "] : GL(@"(brak wykonawcy i albumu - dotknij, aby uzupełnić)");
        c.detailTextLabel.text = [NSString stringWithFormat:@"%@\n%@", l1, FormatLine(it)];
        c.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    }
    return c;
}

- (void)tableView:(UITableView *)t didSelectRowAtIndexPath:(NSIndexPath *)ip
{
    NSArray *q = [[MIStore shared] queue];
    [t deselectRowAtIndexPath:ip animated:YES];
    if (ip.row >= (NSInteger)[q count]) return;
    NSDictionary *it = [q objectAtIndex:(NSUInteger)ip.row];
    NSString *status = [it objectForKey:@"status"];
    if ([status isEqualToString:@"ready"]) {
        MIEditController *vc = [[MIEditController alloc] initWithItemId:[it objectForKey:@"id"]];
        [self.navigationController pushViewController:vc animated:YES];
        return;
    }
    _sheetItemId = [it objectForKey:@"id"];
    UIActionSheet *s = [[UIActionSheet alloc] initWithTitle:nil delegate:self cancelButtonTitle:nil destructiveButtonTitle:nil otherButtonTitles:nil];
    s.tag = kSheetItem;
    NSMutableArray *acts = [NSMutableArray array];
    if ([status isEqualToString:@"error"] && [[it objectForKey:@"url"] length]) { [s addButtonWithTitle:GL(@"Spróbuj ponownie")]; [acts addObject:@"retry"]; }
    s.destructiveButtonIndex = [s addButtonWithTitle:[status isEqualToString:@"downloading"] ? GL(@"Anuluj pobieranie") : GL(@"Usuń z kolejki")];
    [acts addObject:@"delete"];
    s.cancelButtonIndex = [s addButtonWithTitle:GL(@"Zamknij")];
    _sheetItemActions = acts;
    [s showFromTabBar:self.tabBarController.tabBar];
}

- (void)tableView:(UITableView *)t commitEditingStyle:(UITableViewCellEditingStyle)style forRowAtIndexPath:(NSIndexPath *)ip
{
    if (style != UITableViewCellEditingStyleDelete) return;
    NSArray *q = [[MIStore shared] queue];
    if (ip.row >= (NSInteger)[q count]) return;
    NSDictionary *it = [q objectAtIndex:(NSUInteger)ip.row];
    if ([[it objectForKey:@"status"] isEqualToString:@"downloading"]) [[MIDownloads shared] cancelId:[it objectForKey:@"id"]];
    else [[MIStore shared] deleteItemId:[it objectForKey:@"id"]];
    if ([[[MIStore shared] queue] count] == 0) {
        [_table setEditing:NO animated:NO];
        _pendingReloadWhileEditing = NO;
        [self refresh];
    } else {
        [t deleteRowsAtIndexPaths:[NSArray arrayWithObject:ip] withRowAnimation:UITableViewRowAnimationFade];
        _table.tableHeaderView = [self headerView];
        [self updateBadge];
    }
}

/* ---------- dodawanie ---------- */

- (void)addTapped
{
    UIActionSheet *s = [[UIActionSheet alloc] initWithTitle:nil delegate:self cancelButtonTitle:nil destructiveButtonTitle:nil otherButtonTitles:nil];
    s.tag = kSheetAdd;
    [s addButtonWithTitle:GL(@"Pobierz z linku")];
    [s addButtonWithTitle:GL(@"Pliki z folderu aplikacji")];
    s.cancelButtonIndex = [s addButtonWithTitle:GL(@"Anuluj")];
    [s showFromTabBar:self.tabBarController.tabBar];
}

- (void)actionSheet:(UIActionSheet *)s clickedButtonAtIndex:(NSInteger)i
{
    if (i == s.cancelButtonIndex || i < 0) return;
    if (s.tag == kSheetAdd) {
        if (i == 0) [self askLink]; else [self showFiles];
    } else if (s.tag == kSheetItem) {
        if (i >= (NSInteger)[_sheetItemActions count]) return;
        NSString *act = [_sheetItemActions objectAtIndex:(NSUInteger)i];
        NSString *iid = _sheetItemId;
        if ([act isEqualToString:@"retry"]) [[MIDownloads shared] retryId:iid];
        else {
            NSDictionary *it = [[MIStore shared] itemWithId:iid];
            if ([[it objectForKey:@"status"] isEqualToString:@"downloading"]) [[MIDownloads shared] cancelId:iid];
            else [[MIStore shared] deleteItemId:iid];
        }
    }
}

- (void)showFiles
{
    MIFilesController *vc = [[MIFilesController alloc] init];
    [self.navigationController pushViewController:vc animated:YES];
}

- (void)askLink
{
    UIAlertView *a = [[UIAlertView alloc] initWithTitle:GL(@"Pobierz z linku") message:GL(@"Wklej bezpośredni link do pliku MP3, M4A albo WAV.")
                                               delegate:self cancelButtonTitle:GL(@"Anuluj") otherButtonTitles:GL(@"Pobierz"), nil];
    a.tag = kAlertLink;
    a.alertViewStyle = UIAlertViewStylePlainTextInput;
    UITextField *f = [a textFieldAtIndex:0];
    f.keyboardType = UIKeyboardTypeURL;
    f.autocapitalizationType = UITextAutocapitalizationTypeNone;
    f.autocorrectionType = UITextAutocorrectionTypeNo;
    f.clearButtonMode = UITextFieldViewModeWhileEditing;
    f.placeholder = @"http://";
    NSString *clip = [UIPasteboard generalPasteboard].string;
    NSString *low = [clip lowercaseString];
    if ([low hasPrefix:@"http://"] || [low hasPrefix:@"https://"]) f.text = clip;
    [a show];
}

/* ---------- import ---------- */

- (void)importTapped
{
    NSUInteger n = [[MIStore shared] readyCount];
    if (n == 0) return;
    if (![MILibrary databaseExists]) {
        MIAlert(GL(@"Nie znaleziono biblioteki Muzyki"), GL(@"Nie ma pliku /var/mobile/Media/iTunes_Control/iTunes/MediaLibrary.sqlitedb. Ta aplikacja działa tylko na iPhonie z iOS 6 i musi być zainstalowana jako paczka .deb (przez Cydię), nie jako .ipa."));
        return;
    }
    NSString *msg = [NSString stringWithFormat:GL(@"Dodać %d utworów do aplikacji Muzyka? Przed zapisem aplikacja zrobi kopię zapasową biblioteki."), (int)n];
    UIAlertView *a = [[UIAlertView alloc] initWithTitle:GL(@"Dodaj do Muzyki") message:msg delegate:self cancelButtonTitle:GL(@"Anuluj") otherButtonTitles:GL(@"Dodaj"), nil];
    a.tag = kAlertImport;
    [a show];
}

- (void)showBusy:(NSString *)text
{
    UIView *host = self.tabBarController.view ? self.tabBarController.view : self.view;
    _busy = [[UIView alloc] initWithFrame:host.bounds];
    _busy.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _busy.backgroundColor = [UIColor colorWithWhite:0.0f alpha:0.72f];
    UIActivityIndicatorView *sp = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhiteLarge];
    sp.center = CGPointMake(host.bounds.size.width / 2, host.bounds.size.height / 2 - 20);
    sp.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin;
    [sp startAnimating];
    [_busy addSubview:sp];
    UILabel *l = [[UILabel alloc] initWithFrame:CGRectMake(20, host.bounds.size.height / 2 + 14, host.bounds.size.width - 40, 60)];
    l.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin;
    l.backgroundColor = [UIColor clearColor];
    l.textColor = [UIColor whiteColor];
    l.textAlignment = NSTextAlignmentCenter;
    l.numberOfLines = 3;
    l.font = [UIFont boldSystemFontOfSize:16.0f];
    l.text = text;
    [_busy addSubview:l];
    [host addSubview:_busy];
}

- (void)hideBusy { [_busy removeFromSuperview]; _busy = nil; }

- (void)runImport
{
    NSMutableArray *items = [NSMutableArray array];
    for (NSDictionary *it in [[MIStore shared] queue]) if ([[it objectForKey:@"status"] isEqualToString:@"ready"]) [items addObject:[it copy]];
    [self showBusy:GL(@"Zapisuję w bibliotece Muzyki...\nNie zamykaj aplikacji.")];
    UIBackgroundTaskIdentifier bg = [[UIApplication sharedApplication] beginBackgroundTaskWithExpirationHandler:nil];
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSDictionary *out = [MILibrary importItems:items];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self finishImport:out];
            if (bg != UIBackgroundTaskInvalid) [[UIApplication sharedApplication] endBackgroundTask:bg];
        });
    });
}

- (void)finishImport:(NSDictionary *)out
{
    MIStore *store = [MIStore shared];
    [self hideBusy];
    _lastReport = [out objectForKey:@"report"];
    store.lastReport = _lastReport;
    NSMutableArray *errors = [NSMutableArray array];
    int ok = 0, total = 0;
    for (NSDictionary *r in [out objectForKey:@"results"]) {
        total++;
        NSString *iid = [r objectForKey:@"id"];
        if ([[r objectForKey:@"ok"] boolValue]) {
            ok++;
            NSDictionary *rec = [r objectForKey:@"record"];
            if (rec) [store addImported:rec];
            if (store.removeAfterImport) [store deleteItemId:iid];
            else [store updateItemId:iid values:[NSDictionary dictionaryWithObject:@"done" forKey:@"status"] save:YES];
        } else {
            NSDictionary *it = [store itemWithId:iid];
            [errors addObject:[NSString stringWithFormat:@"%@: %@", [it objectForKey:@"title"], [r objectForKey:@"error"]]];
        }
    }
    [store save];
    if (ok > 0 && store.restartMusic) [MILibrary closeMusicApp];
    NSMutableString *msg = [NSMutableString stringWithFormat:GL(@"Dodano %d z %d utworów."), ok, total];
    if (ok > 0) [msg appendString:GL(@"\n\nOtwórz aplikację Muzyka. Jeśli utworów nie widać, uruchom ją ponownie albo użyj \"Zrestartuj (respring)\" w Ustawieniach.")];
    if ([errors count]) {
        [msg appendString:@"\n\n"];
        [msg appendString:[errors objectAtIndex:0]];
        if ([errors count] > 1) [msg appendFormat:GL(@"\n(+ %d innych błędów)"), (int)[errors count] - 1];
    }
    UIAlertView *a = [[UIAlertView alloc] initWithTitle:ok > 0 ? GL(@"Gotowe") : GL(@"Nie udało się") message:msg delegate:self
                                      cancelButtonTitle:@"OK" otherButtonTitles:GL(@"Pokaż raport"), nil];
    a.tag = kAlertResult;
    [a show];
    [self refresh];
}

- (void)alertView:(UIAlertView *)a clickedButtonAtIndex:(NSInteger)i
{
    if (a.tag == kAlertLink) {
        if (i == a.cancelButtonIndex) return;
        NSString *err = nil;
        NSString *address = [[a textFieldAtIndex:0] text];
        if (![[MIDownloads shared] startWithAddress:address error:&err]) MIAlert(nil, err);
    } else if (a.tag == kAlertImport) {
        if (i != a.cancelButtonIndex) [self runImport];
    } else if (a.tag == kAlertResult) {
        if (i != a.cancelButtonIndex) [MITextController presentTitle:GL(@"Raport") text:_lastReport from:self.tabBarController ? (UIViewController *)self.tabBarController : self];
    }
}

@end
