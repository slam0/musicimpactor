#import "MIViews.h"
#import "MITheme.h"
#import "MILang.h"

static UIColor *C(int r, int g, int b, CGFloat a)
{
    return [UIColor colorWithRed:r / 255.0f green:g / 255.0f blue:b / 255.0f alpha:a];
}

static void Gradient(CGContextRef c, CGRect r, UIColor *top, UIColor *bottom)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    NSArray *cols = [NSArray arrayWithObjects:(__bridge id)top.CGColor, (__bridge id)bottom.CGColor, nil];
    CGGradientRef g = CGGradientCreateWithColors(cs, (__bridge CFArrayRef)cols, NULL);
    CGContextSaveGState(c);
    CGContextClipToRect(c, r);
    CGContextDrawLinearGradient(c, g, CGPointMake(0, r.origin.y), CGPointMake(0, r.origin.y + r.size.height), 0);
    CGContextRestoreGState(c);
    CGGradientRelease(g);
    CGColorSpaceRelease(cs);
}

/* ---------- tło komórki ---------- */

@interface MIGlossBackground : UIView
@property (nonatomic) BOOL selected;
@end

@implementation MIGlossBackground
- (id)initWithFrame:(CGRect)f
{
    if ((self = [super initWithFrame:f])) {
        self.opaque = YES;
        self.contentMode = UIViewContentModeRedraw;
    }
    return self;
}
- (void)drawRect:(CGRect)rect
{
    CGContextRef c = UIGraphicsGetCurrentContext();
    CGRect b = self.bounds;
    CGFloat half = (CGFloat)((int)(b.size.height / 2.0f));
    const MIPalette *p = MIP();
    if (_selected) {
        Gradient(c, CGRectMake(0, 0, b.size.width, half), MIRGB(p->selT1, 1), MIRGB(p->selT2, 1));
        Gradient(c, CGRectMake(0, half, b.size.width, b.size.height - half), MIRGB(p->selB1, 1), MIRGB(p->selB2, 1));
    } else {
        Gradient(c, CGRectMake(0, 0, b.size.width, half), MIRGB(p->cellT1, 1), MIRGB(p->cellT2, 1));
        Gradient(c, CGRectMake(0, half, b.size.width, b.size.height - half), MIRGB(p->cellB1, 1), MIRGB(p->cellB2, 1));
    }
    [C(255, 255, 255, p->cellHiAlpha) setFill];
    CGContextFillRect(c, CGRectMake(0, 0, b.size.width, 1));
    [C(0, 0, 0, p->cellLoAlpha) setFill];
    CGContextFillRect(c, CGRectMake(0, b.size.height - 1, b.size.width, 1));
}
@end

@implementation MICell

+ (MICell *)cellForTable:(UITableView *)table identifier:(NSString *)ident
{
    MICell *cell = [table dequeueReusableCellWithIdentifier:ident];
    if (!cell) cell = [[MICell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:ident];
    return cell;
}

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuse
{
    if ((self = [super initWithStyle:style reuseIdentifier:reuse])) {
        self.backgroundView = [[MIGlossBackground alloc] initWithFrame:self.bounds];
        MIGlossBackground *sel = [[MIGlossBackground alloc] initWithFrame:self.bounds];
        sel.selected = YES;
        self.selectedBackgroundView = sel;
        self.textLabel.textColor = MITextColor();
        MIStyleShadow(self.textLabel);
        self.textLabel.font = [UIFont boldSystemFontOfSize:17.0f];
        self.textLabel.backgroundColor = [UIColor clearColor];
        self.detailTextLabel.textColor = MISubtleColor();
        self.detailTextLabel.font = [UIFont systemFontOfSize:13.0f];
        self.detailTextLabel.backgroundColor = [UIColor clearColor];
    }
    return self;
}
@end

/* ---------- nagłówek sekcji ---------- */

@interface MISectionHeader ()
{
    NSString *_title, *_right;
}
@end

@implementation MISectionHeader
- (id)initWithTitle:(NSString *)title right:(NSString *)right
{
    if ((self = [super initWithFrame:CGRectMake(0, 0, 320, 26)])) {
        _title = [title copy];
        _right = [right copy];
        self.opaque = NO;
        self.backgroundColor = [UIColor clearColor];
        self.autoresizingMask = UIViewAutoresizingFlexibleWidth;
        self.contentMode = UIViewContentModeRedraw;
    }
    return self;
}
- (void)drawRect:(CGRect)rect
{
    CGContextRef c = UIGraphicsGetCurrentContext();
    CGRect b = self.bounds;
    const MIPalette *p = MIP();
    Gradient(c, b, MIRGB(p->hdrT, 0.97f), MIRGB(p->hdrB, 0.97f));
    [C(255, 255, 255, p->light ? 0.35f : 0.10f) setFill];
    CGContextFillRect(c, CGRectMake(0, 0, b.size.width, 1));
    [C(0, 0, 0, 0.8f) setFill];
    CGContextFillRect(c, CGRectMake(0, b.size.height - 1, b.size.width, 1));
    UIFont *f = [UIFont boldSystemFontOfSize:13.0f];
    CGContextSetShadowWithColor(c, CGSizeMake(0, -1), 0, C(0, 0, 0, p->light ? 0.45f : 0.9f).CGColor);
    [MIRGB(p->hdrTitle, 1) set];
    [_title drawInRect:CGRectMake(12, 5, b.size.width - 100, 18) withFont:f lineBreakMode:NSLineBreakByTruncatingTail];
    if ([_right length]) {
        [MIRGB(p->hdrRight, 1) set];
        [_right drawInRect:CGRectMake(b.size.width - 160, 5, 148, 18) withFont:f lineBreakMode:NSLineBreakByTruncatingTail alignment:NSTextAlignmentRight];
    }
}
@end

UIView *MIHeaderForTitle(NSString *title, NSString *right)
{
    return [[MISectionHeader alloc] initWithTitle:title right:right];
}

/* ---------- klawiatura ---------- */

UIToolbar *MIKeyboardToolbar(id target, SEL doneAction)
{
    UIToolbar *tb = [[UIToolbar alloc] initWithFrame:CGRectMake(0, 0, 320, 38)];
    tb.barStyle = UIBarStyleBlack;
    tb.translucent = YES;
    UIBarButtonItem *flex = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:nil action:nil];
    UIBarButtonItem *done = [[UIBarButtonItem alloc] initWithTitle:GL(@"Gotowe") style:UIBarButtonItemStyleDone target:target action:doneAction];
    tb.items = [NSArray arrayWithObjects:flex, done, nil];
    return tb;
}

UIImage *MICheckImage(BOOL on)
{
    CGFloat s = 26.0f;
    UIGraphicsBeginImageContextWithOptions(CGSizeMake(s, s), NO, 0.0f);
    UIBezierPath *p = [UIBezierPath bezierPath];
    [p moveToPoint:CGPointMake(5, 14)];
    [p addLineToPoint:CGPointMake(10.5f, 19.5f)];
    [p addLineToPoint:CGPointMake(21, 7)];
    p.lineWidth = 4.0f;
    p.lineCapStyle = kCGLineCapRound;
    p.lineJoinStyle = kCGLineJoinRound;
    CGContextRef c = UIGraphicsGetCurrentContext();
    if (on) {
        CGContextSetShadowWithColor(c, CGSizeMake(0, -1), 0, C(0, 0, 0, 0.6f).CGColor);
        [[UIColor whiteColor] setStroke];
    } else {
        [MISubtleColor() setStroke];
    }
    [p stroke];
    UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return img;
}

void MIOpenURL(NSString *url)
{
    Class urlClass = NSClassFromString(@"NSURL");
    id u = [urlClass URLWithString:url];
    if (u) [[UIApplication sharedApplication] openURL:u];
}

id MIURL(NSString *string)
{
    Class urlClass = NSClassFromString(@"NSURL");
    return string ? [urlClass URLWithString:string] : nil;
}

id MIFileURL(NSString *path)
{
    Class urlClass = NSClassFromString(@"NSURL");
    return path ? [urlClass fileURLWithPath:path] : nil;
}

NSString *MIBytesString(double bytes)
{
    NSString *s;
    if (bytes < 1000.0) s = [NSString stringWithFormat:@"%d B", (int)bytes];
    else if (bytes < 1000000.0) s = [NSString stringWithFormat:@"%.0f KB", bytes / 1000.0];
    else if (bytes < 1000000000.0) s = [NSString stringWithFormat:@"%.1f MB", bytes / 1000000.0];
    else s = [NSString stringWithFormat:@"%.2f GB", bytes / 1000000000.0];
    return MILanguage() == 0 || MILanguage() == 2 ? [s stringByReplacingOccurrencesOfString:@"." withString:@","] : s;
}

NSString *MIDurationString(long ms)
{
    int total = (int)((double)ms / 1000.0 + 0.5);
    int m = total / 60;           /* dzielenie przez stałą: kompilator zamienia na mnożenie */
    int s = total - m * 60;
    if (ms <= 0) return @"–:––";
    return [NSString stringWithFormat:@"%d:%02d", m, s];
}

void MIAlert(NSString *title, NSString *message)
{
    UIAlertView *a = [[UIAlertView alloc] initWithTitle:title message:message delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil];
    [a show];
}

UIColor *MIErrorColor(void)
{
    return MIP()->light ? C(180, 20, 20, 1) : C(255, 120, 110, 1);
}
