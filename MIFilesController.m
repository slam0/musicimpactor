#import "MIFilesController.h"
#import "MIStore.h"
#import "MITheme.h"
#import "MILang.h"
#import "MIViews.h"

@interface MIFilesController ()
{
    UITableView *_table;
    UILabel *_hint;
    NSMutableArray *_files;          /* słowniki: path, name, size */
}
@end

@implementation MIFilesController

- (id)init
{
    if ((self = [super init])) self.title = GL(@"Pliki z folderu");
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

    CGRect b = self.view.bounds;
    _hint = [[UILabel alloc] initWithFrame:CGRectMake(24, 50, b.size.width - 48, 220)];
    _hint.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    _hint.backgroundColor = [UIColor clearColor];
    _hint.textAlignment = NSTextAlignmentCenter;
    _hint.numberOfLines = 0;
    _hint.font = [UIFont systemFontOfSize:15.0f];
    _hint.textColor = MISubtleColor();
    _hint.text = GL(@"Nie ma tu żadnych plików.\n\nPodłącz telefon do komputera, otwórz iTunes → telefon → Aplikacje → Udostępnianie plików → Music Impactor i przeciągnij tam pliki MP3, M4A albo WAV (mogą być w podfolderach). Potem wróć tutaj.");
    MIStyleShadow(_hint);
    [self.view addSubview:_hint];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    [self scan];
}

static BOOL SkipName(NSString *n)
{
    NSString *l = [n lowercaseString];
    if ([l hasPrefix:@"."]) return YES;
    if ([l isEqualToString:@"musicimpactor.json"] || [l isEqualToString:@"raport.txt"] || [l isEqualToString:@"musicimpactor.json.uszkodzony"]) return YES;
    NSString *ext = [l pathExtension];
    NSArray *skip = [NSArray arrayWithObjects:@"json", @"txt", @"plist", @"sqlitedb", @"sqlite", @"db", @"part", @"jpg", @"jpeg", @"png", @"gif", @"pdf", @"html", @"htm", @"zip", nil];
    return [skip containsObject:ext];
}

- (void)scanDir:(NSString *)dir depth:(int)depth into:(NSMutableArray *)out
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray *names = [fm contentsOfDirectoryAtPath:dir error:NULL];
    for (NSString *n in names) {
        NSString *p = [dir stringByAppendingPathComponent:n];
        BOOL isDir = NO;
        if (![fm fileExistsAtPath:p isDirectory:&isDir]) continue;
        if (isDir) {
            if (depth < 4 && ![n hasPrefix:@"."] && !(depth == 0 && ([n isEqualToString:@"Kolejka"] || [n isEqualToString:@"Kopie"])))
                [self scanDir:p depth:depth + 1 into:out];
            continue;
        }
        if (SkipName(n)) continue;
        NSDictionary *at = [fm attributesOfItemAtPath:p error:NULL];
        unsigned long long size = [[at objectForKey:@"NSFileSize"] unsignedLongLongValue];
        if (size < 16) continue;
        [out addObject:[NSDictionary dictionaryWithObjectsAndKeys:p, @"path", n, @"name", [NSNumber numberWithDouble:(double)size], @"size", nil]];
    }
}

- (void)scan
{
    NSMutableArray *out = [NSMutableArray array];
    [self scanDir:[[MIStore shared] documents] depth:0 into:out];
    [out sortUsingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) {
        return [[a objectForKey:@"path"] compare:[b objectForKey:@"path"] options:NSCaseInsensitiveSearch | NSNumericSearch];
    }];
    _files = out;
    _hint.hidden = [out count] > 0;
    _table.hidden = [out count] == 0;
    self.navigationItem.rightBarButtonItem = [out count] ? MIBarButton(GL(@"Dodaj wszystkie"), self, @selector(addAll)) : nil;
    [_table reloadData];
}

- (NSInteger)tableView:(UITableView *)t numberOfRowsInSection:(NSInteger)s { return (NSInteger)[_files count]; }

- (UITableViewCell *)tableView:(UITableView *)t cellForRowAtIndexPath:(NSIndexPath *)ip
{
    MICell *c = [MICell cellForTable:t identifier:@"f"];
    NSDictionary *f = [_files objectAtIndex:(NSUInteger)ip.row];
    c.textLabel.text = [f objectForKey:@"name"];
    NSString *rel = [[f objectForKey:@"path"] stringByReplacingOccurrencesOfString:[[MIStore shared] documents] withString:@""];
    c.detailTextLabel.text = [NSString stringWithFormat:@"%@ · %@", MIBytesString([[f objectForKey:@"size"] doubleValue]), [rel stringByDeletingLastPathComponent]];
    c.accessoryType = UITableViewCellAccessoryNone;
    return c;
}

- (void)tableView:(UITableView *)t didSelectRowAtIndexPath:(NSIndexPath *)ip
{
    [t deselectRowAtIndexPath:ip animated:YES];
    NSDictionary *f = [_files objectAtIndex:(NSUInteger)ip.row];
    NSString *err = nil;
    if ([[MIStore shared] addFileAtPath:[f objectForKey:@"path"] displayName:[f objectForKey:@"name"] move:YES error:&err]) {
        [self scan];
        [self.navigationController popViewControllerAnimated:YES];
    } else {
        MIAlert([f objectForKey:@"name"], err);
    }
}

- (void)addAll
{
    NSMutableArray *errors = [NSMutableArray array];
    int ok = 0;
    for (NSDictionary *f in [_files copy]) {
        NSString *err = nil;
        if ([[MIStore shared] addFileAtPath:[f objectForKey:@"path"] displayName:[f objectForKey:@"name"] move:YES error:&err]) ok++;
        else [errors addObject:[NSString stringWithFormat:@"%@: %@", [f objectForKey:@"name"], err]];
    }
    [self scan];
    if ([errors count]) {
        NSMutableString *m = [NSMutableString stringWithFormat:GL(@"Dodano %d plików do kolejki, %d pominięto:\n"), ok, (int)[errors count]];
        for (NSUInteger i = 0; i < [errors count] && i < 4; i++) [m appendFormat:@"\n%@", [errors objectAtIndex:i]];
        if ([errors count] > 4) [m appendFormat:GL(@"\n(+ %d innych)"), (int)[errors count] - 4];
        MIAlert(nil, m);
    } else if (ok > 0) {
        [self.navigationController popViewControllerAnimated:YES];
    }
}

@end
