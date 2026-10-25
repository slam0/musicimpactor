#import "MIImportedController.h"
#import "MIStore.h"
#import "MILibrary.h"
#import "MITheme.h"
#import "MILang.h"
#import "MIViews.h"
#import "MITextController.h"

/* AVAudioPlayer pobieramy w czasie działania (NSClassFromString), tak jak klasy sieciowe */
@protocol MIPlayerLike <NSObject>
- (id)initWithContentsOfURL:(id)url error:(NSError **)outError;
- (BOOL)prepareToPlay;
- (BOOL)play;
- (void)stop;
- (NSTimeInterval)duration;
@end

@interface MIImportedController ()
{
    id _player;
    UITableView *_table;
    UIView *_empty;
    UIView *_busy;
    NSDictionary *_pending;
}
@end

@implementation MIImportedController

- (id)init
{
    if ((self = [super init])) self.title = GL(@"Zaimportowane");
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
    _table.rowHeight = 60;
    MIStyleTable(_table);
    [self.view addSubview:_table];

    CGRect b = self.view.bounds;
    UILabel *l = [[UILabel alloc] initWithFrame:CGRectMake(24, 60, b.size.width - 48, 200)];
    l.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    l.backgroundColor = [UIColor clearColor];
    l.textAlignment = NSTextAlignmentCenter;
    l.numberOfLines = 0;
    l.font = [UIFont systemFontOfSize:15.0f];
    l.textColor = MISubtleColor();
    l.text = GL(@"Tu pojawią się utwory dodane do Muzyki przez tę aplikację.\n\nStąd możesz je też usunąć z biblioteki (przesuń palcem po wierszu albo dotknij Edytuj).");
    MIStyleShadow(l);
    _empty = l;
    [self.view addSubview:_empty];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [self refresh];
}

- (NSArray *)rows
{
    NSMutableArray *r = [NSMutableArray arrayWithArray:[[MIStore shared] imported]];
    NSArray *rev = [[r reverseObjectEnumerator] allObjects];       /* najnowsze na górze */
    return rev;
}

- (void)refresh
{
    NSUInteger n = [[self rows] count];
    _empty.hidden = n > 0;
    _table.hidden = n == 0;
    if (n == 0) { [_table setEditing:NO animated:NO]; self.navigationItem.leftBarButtonItem = nil; }
    else self.navigationItem.leftBarButtonItem = MIBarButton(_table.editing ? GL(@"Gotowe") : GL(@"Edytuj"), self, @selector(editTapped));
    [_table reloadData];
}

- (void)editTapped
{
    [_table setEditing:!_table.editing animated:YES];
    self.navigationItem.leftBarButtonItem = MIBarButton(_table.editing ? GL(@"Gotowe") : GL(@"Edytuj"), self, @selector(editTapped));
}

- (NSInteger)tableView:(UITableView *)t numberOfRowsInSection:(NSInteger)s { return (NSInteger)[[self rows] count]; }

- (UITableViewCell *)tableView:(UITableView *)t cellForRowAtIndexPath:(NSIndexPath *)ip
{
    MICell *c = [MICell cellForTable:t identifier:@"imp"];
    NSArray *rows = [self rows];
    if (ip.row >= (NSInteger)[rows count]) return c;
    NSDictionary *r = [rows objectAtIndex:(NSUInteger)ip.row];
    c.textLabel.text = [r objectForKey:@"title"];
    NSMutableArray *who = [NSMutableArray array];
    if ([[r objectForKey:@"artist"] length]) [who addObject:[r objectForKey:@"artist"]];
    if ([[r objectForKey:@"album"] length]) [who addObject:[r objectForKey:@"album"]];
    NSString *line = [who count] ? [who componentsJoinedByString:@" — "] : @"";
    c.detailTextLabel.text = [NSString stringWithFormat:@"%@%@%@", line, [line length] ? @" · " : @"", MIDateString([[r objectForKey:@"added"] doubleValue])];
    c.accessoryType = UITableViewCellAccessoryNone;
    return c;
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
    if (_player) { [_player stop]; _player = nil; }
}

/* Dotknięcie utworu: odtwarza plik własnym odtwarzaczem aplikacji. Pokazuje, czy winny jest plik, czy baza biblioteki. */
- (void)tableView:(UITableView *)t didSelectRowAtIndexPath:(NSIndexPath *)ip
{
    [t deselectRowAtIndexPath:ip animated:YES];
    if (_player) { [_player stop]; _player = nil; return; }
    NSArray *rows = [self rows];
    if (ip.row >= (NSInteger)[rows count]) return;
    NSString *rel = [[rows objectAtIndex:(NSUInteger)ip.row] objectForKey:@"rel"];
    if (![rel length]) return;
    NSString *path = [@"/var/mobile/Media/iTunes_Control/Music" stringByAppendingPathComponent:rel];
    NSDictionary *at = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL];
    if (!at) { MIAlert(GL(@"Test odtwarzania"), [NSString stringWithFormat:GL(@"Nie ma pliku:\n%@"), path]); return; }
    NSString *info = [NSString stringWithFormat:@"%@\n%@ · %@ · %o", path, MIBytesString([[at objectForKey:@"NSFileSize"] doubleValue]),
                      [at objectForKey:@"NSFileOwnerAccountName"] ? [at objectForKey:@"NSFileOwnerAccountName"] : @"?",
                      (unsigned)[[at objectForKey:@"NSFilePosixPermissions"] unsignedIntegerValue]];
    Class cls = NSClassFromString(@"AVAudioPlayer");
    if (!cls) { MIAlert(GL(@"Test odtwarzania"), GL(@"Odtwarzacz systemowy nie jest dostępny")); return; }
    NSError *err = nil;
    id<MIPlayerLike> p = [cls alloc];
    p = [p initWithContentsOfURL:MIFileURL(path) error:&err];
    if (!p) {
        NSString *why = err ? [NSString stringWithFormat:@"%@ %d", [err domain], (int)[err code]] : @"?";
        MIAlert(GL(@"Test odtwarzania"), [NSString stringWithFormat:GL(@"Odtwarzacz nie otworzył pliku.\nBłąd: %@\n\n%@"), why, info]);
        return;
    }
    [p prepareToPlay];
    [p play];
    _player = p;
    MIAlert(GL(@"Test odtwarzania"), [NSString stringWithFormat:GL(@"Plik się odtwarza, więc sam plik jest dobry (czas: %d s).\nJeśli Muzyka go nie odtwarza, problem jest w bazie biblioteki.\nDotknij utworu jeszcze raz, żeby zatrzymać."), (int)[p duration]]);
}

- (void)tableView:(UITableView *)t commitEditingStyle:(UITableViewCellEditingStyle)style forRowAtIndexPath:(NSIndexPath *)ip
{
    if (style != UITableViewCellEditingStyleDelete) return;
    NSArray *rows = [self rows];
    if (ip.row >= (NSInteger)[rows count]) return;
    _pending = [rows objectAtIndex:(NSUInteger)ip.row];
    UIActionSheet *s = [[UIActionSheet alloc] initWithTitle:GL(@"Usunąć ten utwór z Muzyki? Plik i wpisy w bibliotece zostaną skasowane.") delegate:self
                                          cancelButtonTitle:GL(@"Anuluj") destructiveButtonTitle:GL(@"Usuń z Muzyki") otherButtonTitles:nil];
    [s showFromTabBar:self.tabBarController.tabBar];
}

- (void)actionSheet:(UIActionSheet *)s clickedButtonAtIndex:(NSInteger)i
{
    if (i != s.destructiveButtonIndex || !_pending) { _pending = nil; [_table reloadData]; return; }
    NSDictionary *rec = _pending;
    _pending = nil;
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
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSString *report = nil;
        BOOL ok = [MILibrary removeRecord:rec report:&report];
        dispatch_async(dispatch_get_main_queue(), ^{
            [_busy removeFromSuperview];
            _busy = nil;
            if (ok) {
                [[MIStore shared] removeImportedPid:[rec objectForKey:@"pid"]];
                [self refresh];
            } else {
                [MITextController presentTitle:GL(@"Nie udało się usunąć") text:report from:self.tabBarController];
                [_table reloadData];
            }
        });
    });
}

@end
