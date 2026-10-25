#import "MIEditController.h"
#import "MIStore.h"
#import "MITheme.h"
#import "MILang.h"
#import "MIViews.h"

#define kFieldTag 500

typedef struct { NSString *key; NSString *label; int numeric; } MIField;

static const int kFieldCount = 10;

static MIField FieldAt(int i)
{
    static const MIField f[10] = {
        { @"title", @"Tytuł", 0 }, { @"artist", @"Wykonawca", 0 }, { @"album", @"Album", 0 }, { @"albumartist", @"Wyk. albumu", 0 },
        { @"genre", @"Gatunek", 0 }, { @"year", @"Rok", 1 }, { @"track", @"Nr utworu", 1 }, { @"tracks", @"Utworów", 1 },
        { @"disc", @"Nr dysku", 1 }, { @"discs", @"Dysków", 1 }
    };
    return f[i];
}

@interface MIEditController ()
{
    NSString *_iid;
    UITableView *_table;
    NSMutableDictionary *_values;
    UITextField *_active;
}
@end

@implementation MIEditController

- (id)initWithItemId:(NSString *)iid
{
    if ((self = [super init])) {
        _iid = [iid copy];
        self.title = GL(@"Tagi");
        NSDictionary *it = [[MIStore shared] itemWithId:iid];
        _values = [NSMutableDictionary dictionary];
        for (int i = 0; i < kFieldCount; i++) {
            MIField f = FieldAt(i);
            id v = [it objectForKey:f.key];
            if (f.numeric) {
                int n = [v intValue];
                [_values setObject:(n > 0 ? [NSString stringWithFormat:@"%d", n] : @"") forKey:f.key];
            } else {
                [_values setObject:([v isKindOfClass:[NSString class]] ? v : @"") forKey:f.key];
            }
        }
    }
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
    _table.rowHeight = 46;
    MIStyleTable(_table);
    [self.view addSubview:_table];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardShown:) name:@"UIKeyboardWillShowNotification" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardHidden:) name:@"UIKeyboardWillHideNotification" object:nil];
}

- (void)dealloc { [[NSNotificationCenter defaultCenter] removeObserver:self]; }

- (void)keyboardShown:(NSNotification *)n
{
    CGRect kb = [[[n userInfo] objectForKey:@"UIKeyboardFrameEndUserInfoKey"] CGRectValue];
    kb = [self.view convertRect:[self.view.window convertRect:kb fromWindow:nil] fromView:nil];
    CGFloat overlap = CGRectGetMaxY(self.view.bounds) - kb.origin.y;
    if (overlap < 0) overlap = 0;
    UIEdgeInsets in = _table.contentInset;
    in.bottom = overlap;
    _table.contentInset = in;
    _table.scrollIndicatorInsets = in;
}

- (void)keyboardHidden:(NSNotification *)n
{
    UIEdgeInsets in = _table.contentInset;
    in.bottom = 0;
    _table.contentInset = in;
    _table.scrollIndicatorInsets = in;
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
    [self.view endEditing:YES];
    [self commit];
}

- (void)commit
{
    NSMutableDictionary *vals = [NSMutableDictionary dictionary];
    for (int i = 0; i < kFieldCount; i++) {
        MIField f = FieldAt(i);
        NSString *s = [[_values objectForKey:f.key] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (f.numeric) [vals setObject:[NSNumber numberWithInt:[s intValue]] forKey:f.key];
        else [vals setObject:s ? s : @"" forKey:f.key];
    }
    if (![[vals objectForKey:@"title"] length]) [vals setObject:GL(@"Bez tytułu") forKey:@"title"];
    [[MIStore shared] updateItemId:_iid values:vals save:YES];
}

/* ---------- tabela: 0 pola, 1 informacje, 2 akcje ---------- */

- (NSInteger)numberOfSectionsInTableView:(UITableView *)t { return 3; }

- (NSInteger)tableView:(UITableView *)t numberOfRowsInSection:(NSInteger)s
{
    return s == 0 ? kFieldCount : (s == 1 ? 3 : 2);
}

- (CGFloat)tableView:(UITableView *)t heightForHeaderInSection:(NSInteger)s { return 26; }

- (UIView *)tableView:(UITableView *)t viewForHeaderInSection:(NSInteger)s
{
    NSArray *n = [NSArray arrayWithObjects:@"Tagi", @"Plik", @"Akcje", nil];
    return MIHeaderForTitle(GL([n objectAtIndex:(NSUInteger)s]), nil);
}

- (CGFloat)tableView:(UITableView *)t heightForRowAtIndexPath:(NSIndexPath *)ip
{
    return ip.section == 2 ? 62 : 46;
}

- (UITableViewCell *)tableView:(UITableView *)t cellForRowAtIndexPath:(NSIndexPath *)ip
{
    MICell *c = [MICell cellForTable:t identifier:(ip.section == 0 ? @"field" : @"info")];
    c.accessoryType = UITableViewCellAccessoryNone;
    c.accessoryView = nil;
    c.selectionStyle = ip.section == 2 ? UITableViewCellSelectionStyleBlue : UITableViewCellSelectionStyleNone;
    c.textLabel.textColor = MITextColor();
    c.detailTextLabel.text = nil;
    c.detailTextLabel.numberOfLines = 2;
    UITextField *old = (UITextField *)[c.contentView viewWithTag:kFieldTag];
    if (ip.section == 0) {
        MIField f = FieldAt((int)ip.row);
        c.textLabel.text = GL(f.label);
        c.textLabel.font = [UIFont boldSystemFontOfSize:14.0f];
        UITextField *tf = old;
        if (!tf) {
            tf = [[UITextField alloc] initWithFrame:CGRectMake(112, 0, c.contentView.bounds.size.width - 122, 46)];
            tf.tag = kFieldTag;
            tf.autoresizingMask = UIViewAutoresizingFlexibleWidth;
            tf.backgroundColor = [UIColor clearColor];
            tf.font = [UIFont systemFontOfSize:16.0f];
            tf.textColor = MITextColor();
            tf.clearButtonMode = UITextFieldViewModeWhileEditing;
            tf.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
            tf.delegate = self;
            tf.autocorrectionType = UITextAutocorrectionTypeNo;
            tf.returnKeyType = UIReturnKeyDone;
            [c.contentView addSubview:tf];
        }
        tf.text = [_values objectForKey:f.key];
        tf.placeholder = f.numeric ? @"–" : GL(@"nie ustawiono");
        tf.keyboardType = f.numeric ? UIKeyboardTypeNumberPad : UIKeyboardTypeDefault;
        tf.autocapitalizationType = f.numeric ? UITextAutocapitalizationTypeNone : UITextAutocapitalizationTypeWords;
        tf.inputAccessoryView = f.numeric ? MIKeyboardToolbar(self, @selector(doneTapped)) : nil;
        c.textLabel.hidden = NO;
        old = tf;
        return c;
    }
    old.hidden = YES;
    c.textLabel.font = [UIFont boldSystemFontOfSize:15.0f];
    NSDictionary *it = [[MIStore shared] itemWithId:_iid];
    if (ip.section == 1) {
        if (ip.row == 0) {
            c.textLabel.text = GL(@"Format");
            NSString *ext = [[it objectForKey:@"ext"] uppercaseString];
            int br = [[it objectForKey:@"br"] intValue], sr = [[it objectForKey:@"sr"] intValue];
            NSMutableArray *parts = [NSMutableArray arrayWithObject:ext ? ext : @"?"];
            if (br > 0) [parts addObject:[NSString stringWithFormat:@"%d kb/s", br]];
            if (sr > 0) [parts addObject:[NSString stringWithFormat:@"%d Hz", sr]];
            c.detailTextLabel.text = [parts componentsJoinedByString:@" · "];
        } else if (ip.row == 1) {
            c.textLabel.text = GL(@"Długość i rozmiar");
            c.detailTextLabel.text = [NSString stringWithFormat:@"%@ · %@", MIDurationString([[it objectForKey:@"dur"] longValue]), MIBytesString([[it objectForKey:@"size"] doubleValue])];
        } else {
            c.textLabel.text = GL(@"Nazwa pliku");
            c.detailTextLabel.text = [it objectForKey:@"name"];
        }
    } else {
        c.textLabel.font = [UIFont boldSystemFontOfSize:16.0f];
        if (ip.row == 0) {
            c.textLabel.text = GL(@"Zastosuj do całej kolejki");
            c.detailTextLabel.text = GL(@"Wykonawca, album, wykonawca albumu, rok i gatunek zostaną skopiowane do pozostałych utworów");
        } else {
            c.textLabel.text = GL(@"Usuń z kolejki");
            c.textLabel.textColor = MIRGB(MIP()->danger, 1);
            c.detailTextLabel.text = GL(@"Plik zostanie skasowany z aplikacji");
        }
    }
    return c;
}

- (void)tableView:(UITableView *)t didSelectRowAtIndexPath:(NSIndexPath *)ip
{
    [t deselectRowAtIndexPath:ip animated:YES];
    if (ip.section != 2) return;
    [self.view endEditing:YES];
    if (ip.row == 0) {
        [self commit];
        NSDictionary *src = [[MIStore shared] itemWithId:_iid];
        NSArray *keys = [NSArray arrayWithObjects:@"artist", @"album", @"albumartist", @"year", @"genre", @"tracks", @"discs", nil];
        NSMutableDictionary *vals = [NSMutableDictionary dictionary];
        for (NSString *k in keys) { id v = [src objectForKey:k]; if (v) [vals setObject:v forKey:k]; }
        NSUInteger n = 0;
        for (NSDictionary *it in [[MIStore shared] queue]) {
            if ([[it objectForKey:@"id"] isEqualToString:_iid] || ![[it objectForKey:@"status"] isEqualToString:@"ready"]) continue;
            [[MIStore shared] updateItemId:[it objectForKey:@"id"] values:vals save:NO];
            n++;
        }
        [[MIStore shared] save];
        MIAlert(nil, [NSString stringWithFormat:GL(@"Zmieniono %d utworów w kolejce"), (int)n]);
    } else {
        UIActionSheet *s = [[UIActionSheet alloc] initWithTitle:GL(@"Usunąć ten utwór z kolejki?") delegate:self cancelButtonTitle:GL(@"Anuluj")
                                         destructiveButtonTitle:GL(@"Usuń") otherButtonTitles:nil];
        [s showInView:self.view];
    }
}

- (void)actionSheet:(UIActionSheet *)s clickedButtonAtIndex:(NSInteger)i
{
    if (i == s.destructiveButtonIndex) {
        [[MIStore shared] deleteItemId:_iid];
        [self.navigationController popViewControllerAnimated:YES];
    }
}

/* ---------- pola tekstowe ---------- */

- (NSString *)keyForField:(UITextField *)tf
{
    UIView *v = tf;
    while (v && ![v isKindOfClass:[UITableViewCell class]]) v = v.superview;
    if (!v) return nil;
    NSIndexPath *ip = [_table indexPathForCell:(UITableViewCell *)v];
    if (!ip || ip.section != 0) return nil;
    return FieldAt((int)ip.row).key;
}

- (void)textFieldDidEndEditing:(UITextField *)tf
{
    NSString *k = [self keyForField:tf];
    if (k) [_values setObject:(tf.text ? tf.text : @"") forKey:k];
}

- (BOOL)textFieldShouldReturn:(UITextField *)tf { [tf resignFirstResponder]; return NO; }
- (void)doneTapped { [self.view endEditing:YES]; }

@end
