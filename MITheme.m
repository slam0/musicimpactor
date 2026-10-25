#import "MITheme.h"
#import "MIStore.h"
#import "MILang.h"

/* ---------- paleta i motywy ---------- */

#define NTHEMES 6

static const struct { NSString *name; int accent[3]; int light; } kThemes[NTHEMES] = {
    { @"Grafit",   { 255, 138,   0 }, 0 },
    { @"Błękit",   {  40, 150, 255 }, 0 },
    { @"Czerwień", { 240,  70,  70 }, 0 },
    { @"Zieleń",   {  90, 200,  70 }, 0 },
    { @"Fiolet",   { 170, 110, 255 }, 0 },
    { @"Srebro",   {  22, 118, 220 }, 1 },
};

static MIPalette gPal;
static NSInteger gTheme = -1;

#define SET(dst, r, g, b) do { (dst)[0] = (r); (dst)[1] = (g); (dst)[2] = (b); } while (0)

static void Copy3(int *dst, const int *src) { dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; }

static void Mix(int *dst, const int *a, const int *b, float t)
{
    for (int i = 0; i < 3; i++) dst[i] = (int)(a[i] + (b[i] - a[i]) * t + 0.5f);
}

static void BuildPalette(NSInteger theme, MIPalette *p)
{
    { unsigned char *raw = (unsigned char *)p; for (size_t i = 0; i < sizeof(*p); i++) raw[i] = 0; }
    const int white[3] = { 255, 255, 255 }, black[3] = { 0, 0, 0 };
    p->light = kThemes[theme].light;
    for (int i = 0; i < 3; i++) p->accent[i] = kThemes[theme].accent[i];
    if (!p->light) {
        SET(p->bg, 34, 36, 40);    SET(p->bgHi, 40, 42, 47);  SET(p->bgLo, 30, 32, 36);
        SET(p->barT1, 112, 116, 126); SET(p->barT2, 66, 69, 77); SET(p->barB1, 40, 42, 48); SET(p->barB2, 22, 23, 27);
        SET(p->barTint, 58, 61, 69);
        SET(p->cellT1, 66, 69, 77); SET(p->cellT2, 52, 55, 62); SET(p->cellB1, 44, 46, 52); SET(p->cellB2, 38, 40, 45);
        SET(p->text, 238, 240, 244); SET(p->subtle, 150, 156, 168); SET(p->tabOff, 150, 156, 168);
        SET(p->hdrT, 30, 32, 36);  SET(p->hdrB, 14, 15, 18);  SET(p->hdrRight, 150, 156, 168);
        SET(p->fieldT, 12, 13, 16); SET(p->fieldB, 30, 32, 37); SET(p->fieldText, 255, 255, 255);
        SET(p->tileT, 74, 78, 88); SET(p->tileB, 30, 32, 37); SET(p->tileText, 255, 255, 255);
        SET(p->panelT, 18, 19, 23); SET(p->panelB, 30, 32, 37); SET(p->grid, 255, 255, 255);
        SET(p->sep, 18, 19, 22);
        SET(p->neutT, 104, 108, 118); SET(p->neutB, 44, 46, 52); SET(p->neutBorder, 10, 11, 14);
        SET(p->shadow, 0, 0, 0); SET(p->chrome, 26, 28, 33); SET(p->danger, 255, 115, 115);
        p->gridAlpha = 0.07f; p->shadowAlpha = 0.8f; p->shadowDY = -1.0f; p->cellHiAlpha = 0.14f; p->cellLoAlpha = 0.55f;
        /* delikatne przebarwienie grafitu kolorem motywu (poza pierwszym, czystym grafitem) */
        if (theme != 0) {
            int *group[] = { p->bg, p->bgHi, p->bgLo, p->barT1, p->barT2, p->barB1, p->barB2, p->barTint, p->cellT1, p->cellT2,
                             p->cellB1, p->cellB2, p->hdrT, p->hdrB, p->fieldT, p->fieldB, p->tileT, p->tileB, p->panelT, p->panelB,
                             p->sep, p->neutT, p->neutB, p->neutBorder, p->chrome };
            for (size_t i = 0; i < sizeof(group) / sizeof(group[0]); i++) Mix(group[i], group[i], p->accent, 0.11f);
        }
        Copy3(p->hdrTitle, p->accent);
        Copy3(p->tabOn, p->accent);
        Mix(p->selT1, p->accent, black, 0.40f); Mix(p->selT2, p->accent, black, 0.56f);
        Mix(p->selB1, p->accent, black, 0.64f); Mix(p->selB2, p->accent, black, 0.72f);
    } else {
        SET(p->bg, 222, 225, 231); SET(p->bgHi, 231, 234, 239); SET(p->bgLo, 212, 215, 221);
        SET(p->barT1, 146, 158, 178); SET(p->barT2, 110, 123, 145); SET(p->barB1, 82, 95, 117); SET(p->barB2, 60, 72, 93);
        SET(p->barTint, 96, 109, 131);
        SET(p->cellT1, 253, 253, 254); SET(p->cellT2, 244, 246, 248); SET(p->cellB1, 236, 238, 242); SET(p->cellB2, 228, 231, 235);
        SET(p->text, 30, 33, 40); SET(p->subtle, 98, 106, 120); SET(p->tabOff, 196, 204, 216); SET(p->tabOn, 255, 255, 255);
        SET(p->hdrT, 150, 159, 174); SET(p->hdrB, 116, 125, 141); SET(p->hdrTitle, 255, 255, 255); SET(p->hdrRight, 238, 242, 248);
        SET(p->fieldT, 226, 229, 235); SET(p->fieldB, 250, 251, 253); SET(p->fieldText, 28, 31, 38);
        SET(p->tileT, 252, 253, 254); SET(p->tileB, 206, 211, 220); SET(p->tileText, 30, 33, 40);
        SET(p->panelT, 255, 255, 255); SET(p->panelB, 232, 236, 241); SET(p->grid, 0, 0, 0);
        SET(p->sep, 200, 204, 212);
        SET(p->neutT, 140, 149, 165); SET(p->neutB, 88, 97, 114); SET(p->neutBorder, 62, 70, 86);
        SET(p->shadow, 255, 255, 255); SET(p->chrome, 74, 86, 108); SET(p->danger, 196, 32, 32);
        p->gridAlpha = 0.09f; p->shadowAlpha = 0.9f; p->shadowDY = 1.0f; p->cellHiAlpha = 0.9f; p->cellLoAlpha = 0.16f;
        Mix(p->selT1, p->accent, black, 0.05f); Mix(p->selT2, p->accent, black, 0.15f);
        Mix(p->selB1, p->accent, black, 0.25f); Mix(p->selB2, p->accent, black, 0.30f);
    }
    Mix(p->accentGelT, p->accent, white, 0.25f);
    Mix(p->accentGelB, p->accent, black, 0.18f);
    Mix(p->accentGelBorder, p->accent, black, 0.62f);
}

NSInteger MIThemeCount(void) { return NTHEMES; }

NSInteger MICurrentTheme(void)
{
    if (gTheme < 0) {
        NSInteger t = [MIStore shared].themeIndex;
        gTheme = (t >= 0 && t < NTHEMES) ? t : 0;
        BuildPalette(gTheme, &gPal);
    }
    return gTheme;
}

const MIPalette *MIP(void)
{
    MICurrentTheme();
    return &gPal;
}

static UIImage *gGelCache[NTHEMES][4];
static UIColor *gBgCache[NTHEMES];

void MISetTheme(NSInteger index)
{
    if (index < 0 || index >= NTHEMES) index = 0;
    [MIStore shared].themeIndex = index;
    gTheme = -1;
    MICurrentTheme();
    [[NSNotificationCenter defaultCenter] postNotificationName:MIAppearanceDidChangeNotification object:nil];
}

NSString *MIThemeName(NSInteger index)
{
    if (index < 0 || index >= NTHEMES) return @"";
    return GL(kThemes[index].name);
}

UIColor *MIRGB(const int *c, CGFloat a)
{
    return [UIColor colorWithRed:c[0] / 255.0f green:c[1] / 255.0f blue:c[2] / 255.0f alpha:a];
}

static UIColor *RGBA(int r, int g, int b, CGFloat a)
{
    return [UIColor colorWithRed:r / 255.0f green:g / 255.0f blue:b / 255.0f alpha:a];
}

UIColor *MITextColor(void)   { return MIRGB(MIP()->text, 1); }
UIColor *MISubtleColor(void) { return MIRGB(MIP()->subtle, 1); }
UIColor *MIAccentColor(void) { return MIRGB(MIP()->accent, 1); }
UIColor *MIGreenColor(void)  { return RGBA(88, 200, 70, 1); }

void MIStyleShadow(UILabel *l)
{
    const MIPalette *p = MIP();
    l.shadowColor = MIRGB(p->shadow, p->shadowAlpha);
    l.shadowOffset = CGSizeMake(0, p->shadowDY);
}

/* Tło: teksturowane, delikatne poziome "szczotkowane" linie. */
UIColor *MIBackgroundColor(void)
{
    NSInteger t = MICurrentTheme();
    if (!gBgCache[t]) {
        const MIPalette *p = MIP();
        UIGraphicsBeginImageContextWithOptions(CGSizeMake(8, 4), YES, 0.0f);
        [MIRGB(p->bg, 1) setFill];
        UIRectFill(CGRectMake(0, 0, 8, 4));
        [MIRGB(p->bgHi, 1) setFill];
        UIRectFill(CGRectMake(0, 1, 8, 1));
        [MIRGB(p->bgLo, 1) setFill];
        UIRectFill(CGRectMake(0, 3, 8, 1));
        UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
        UIGraphicsEndImageContext();
        gBgCache[t] = [UIColor colorWithPatternImage:img];
    }
    return gBgCache[t];
}

static void MIFillGradient(CGContextRef c, CGRect r, UIColor *top, UIColor *bottom)
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

/* Błyszczący pasek: jasna górna połowa (odbicie), ciemna dolna. */
static UIImage *MIBarImage(CGFloat height)
{
    const MIPalette *p = MIP();
    UIGraphicsBeginImageContextWithOptions(CGSizeMake(4.0f, height), NO, 0.0f);
    CGContextRef c = UIGraphicsGetCurrentContext();
    CGFloat half = (CGFloat)((int)(height / 2.0f));
    MIFillGradient(c, CGRectMake(0, 0, 4, half), MIRGB(p->barT1, 1), MIRGB(p->barT2, 1));
    MIFillGradient(c, CGRectMake(0, half, 4, height - half), MIRGB(p->barB1, 1), MIRGB(p->barB2, 1));
    [RGBA(255, 255, 255, 1) setFill];
    CGContextSetAlpha(c, 0.35f);
    CGContextFillRect(c, CGRectMake(0, 0, 4, 1));
    CGContextSetAlpha(c, 1.0f);
    [RGBA(0, 0, 0, 0.7f) setFill];
    CGContextFillRect(c, CGRectMake(0, height - 1, 4, 1));
    UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return [img resizableImageWithCapInsets:UIEdgeInsetsMake(0, 1, 0, 1)];
}

static UIImage *MISelectionImage(void)
{
    UIGraphicsBeginImageContextWithOptions(CGSizeMake(20.0f, 49.0f), NO, 0.0f);
    [RGBA(255, 255, 255, MIP()->light ? 0.24f : 0.16f) setFill];
    [[UIBezierPath bezierPathWithRoundedRect:CGRectMake(2, 3, 16, 43) cornerRadius:6] fill];
    UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return [img resizableImageWithCapInsets:UIEdgeInsetsMake(12, 9, 12, 9)];
}

void MIStyleNavigationBar(UINavigationBar *bar)
{
    [bar setBackgroundImage:MIBarImage(44.0f) forBarMetrics:UIBarMetricsDefault];
    bar.tintColor = MIRGB(MIP()->barTint, 1);
    bar.titleTextAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
        [UIColor whiteColor], UITextAttributeTextColor,
        [UIColor colorWithWhite:0.0f alpha:0.85f], UITextAttributeTextShadowColor,
        [NSValue valueWithUIOffset:UIOffsetMake(0, -1)], UITextAttributeTextShadowOffset,
        [UIFont boldSystemFontOfSize:20.0f], UITextAttributeFont, nil];
}

void MIStyleTabBar(UITabBar *bar)
{
    bar.backgroundImage = MIBarImage(49.0f);
    bar.selectionIndicatorImage = MISelectionImage();
}

void MIStyleSearchBar(UISearchBar *bar)
{
    bar.barStyle = UIBarStyleBlack;
    bar.tintColor = MIRGB(MIP()->barTint, 1);
}

/* ---------- ikony zakładek (maski) ---------- */

UIImage *MITabIcon(int kind)
{
    CGFloat s = 30.0f;
    UIGraphicsBeginImageContextWithOptions(CGSizeMake(s, s), NO, 0.0f);
    [[UIColor blackColor] setFill];
    [[UIColor blackColor] setStroke];
    if (kind == 0) {                       /* kolejka: strzałka w dół do tacki */
        UIBezierPath *tray = [UIBezierPath bezierPath];
        [tray moveToPoint:CGPointMake(4.5f, 17)];
        [tray addLineToPoint:CGPointMake(4.5f, 25)];
        [tray addLineToPoint:CGPointMake(25.5f, 25)];
        [tray addLineToPoint:CGPointMake(25.5f, 17)];
        tray.lineWidth = 3.0f;
        tray.lineJoinStyle = kCGLineJoinRound;
        [tray stroke];
        [[UIBezierPath bezierPathWithRoundedRect:CGRectMake(13.5f, 3, 3.2f, 13) cornerRadius:1.2f] fill];
        UIBezierPath *head = [UIBezierPath bezierPath];
        [head moveToPoint:CGPointMake(7.5f, 12)];
        [head addLineToPoint:CGPointMake(22.5f, 12)];
        [head addLineToPoint:CGPointMake(15, 20)];
        [head closePath];
        [head fill];
    } else if (kind == 1) {                /* zaimportowane: nuta */
        [[UIBezierPath bezierPathWithOvalInRect:CGRectMake(4.5f, 18, 9.5f, 7.5f)] fill];
        [[UIBezierPath bezierPathWithOvalInRect:CGRectMake(17, 15.5f, 9.5f, 7.5f)] fill];
        [[UIBezierPath bezierPathWithRect:CGRectMake(12, 6, 2.6f, 15)] fill];
        [[UIBezierPath bezierPathWithRect:CGRectMake(24.4f, 3.5f, 2.6f, 15)] fill];
        UIBezierPath *beam = [UIBezierPath bezierPath];
        [beam moveToPoint:CGPointMake(12, 5)];
        [beam addLineToPoint:CGPointMake(27, 2)];
        [beam addLineToPoint:CGPointMake(27, 7)];
        [beam addLineToPoint:CGPointMake(12, 10)];
        [beam closePath];
        [beam fill];
    } else {                               /* suwaki = ustawienia */
        static const CGFloat knob[3] = { 19, 9, 16 };
        for (int i = 0; i < 3; i++) {
            CGFloat y = 7.0f + i * 8.0f;
            [[UIBezierPath bezierPathWithRoundedRect:CGRectMake(3, y + 1.5f, 24, 2.5f) cornerRadius:1.2f] fill];
            [[UIBezierPath bezierPathWithOvalInRect:CGRectMake(knob[i] - 3.5f, y - 0.5f, 7, 7)] fill];
        }
    }
    UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return img;
}

static UIImage *MITabIconTinted(int kind, UIColor *color)
{
    UIImage *mask = MITabIcon(kind);
    CGRect r = CGRectMake(0, 0, mask.size.width, mask.size.height);
    UIGraphicsBeginImageContextWithOptions(r.size, NO, 0.0f);
    CGContextRef c = UIGraphicsGetCurrentContext();
    [mask drawInRect:r];
    CGContextSetBlendMode(c, kCGBlendModeSourceIn);
    [color setFill];
    CGContextFillRect(c, r);
    UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return img;
}

void MIStyleTabItem(UITabBarItem *item, int kind)
{
    UIColor *off = MIRGB(MIP()->tabOff, 1);
    UIColor *on = MIRGB(MIP()->tabOn, 1);
    [item setFinishedSelectedImage:MITabIconTinted(kind, on) withFinishedUnselectedImage:MITabIconTinted(kind, off)];
    NSValue *offset = [NSValue valueWithUIOffset:UIOffsetMake(0, -1)];
    UIColor *shadow = [UIColor colorWithWhite:0.0f alpha:0.8f];
    [item setTitleTextAttributes:[NSDictionary dictionaryWithObjectsAndKeys:off, UITextAttributeTextColor, shadow, UITextAttributeTextShadowColor, offset, UITextAttributeTextShadowOffset, nil] forState:UIControlStateNormal];
    [item setTitleTextAttributes:[NSDictionary dictionaryWithObjectsAndKeys:on, UITextAttributeTextColor, shadow, UITextAttributeTextShadowColor, offset, UITextAttributeTextShadowOffset, nil] forState:UIControlStateSelected];
}

void MIStyleTable(UITableView *table)
{
    table.backgroundView = nil;
    table.backgroundColor = MIBackgroundColor();
    table.separatorColor = MIRGB(MIP()->sep, 1);
}

/* ---------- błyszczące przyciski ("żel") ---------- */

UIImage *MIGelImage(int style)
{
    NSInteger th = MICurrentTheme();
    if (style < 0 || style > 3) style = 1;
    if (gGelCache[th][style]) return gGelCache[th][style];
    const MIPalette *p = MIP();
    int top[3], bot[3], border[3];
    switch (style) {
        case 0:  Copy3(top, p->accentGelT); Copy3(bot, p->accentGelB); Copy3(border, p->accentGelBorder); break;
        case 2:  SET(top, 235, 90, 90);  SET(bot, 170, 20, 30); SET(border, 90, 8, 14); break;
        case 3:  SET(top, 130, 215, 100); SET(bot, 48, 150, 40); SET(border, 20, 80, 18); break;
        default: Copy3(top, p->neutT); Copy3(bot, p->neutB); Copy3(border, p->neutBorder); break;
    }
    CGFloat w = 24.0f, h = 44.0f;
    UIGraphicsBeginImageContextWithOptions(CGSizeMake(w, h), NO, 0.0f);
    CGContextRef c = UIGraphicsGetCurrentContext();
    CGRect r = CGRectMake(0.5f, 0.5f, w - 1.0f, h - 1.5f);
    UIBezierPath *shape = [UIBezierPath bezierPathWithRoundedRect:r cornerRadius:9.0f];
    CGContextSaveGState(c);
    [shape addClip];
    MIFillGradient(c, CGRectMake(0, 0, w, h), RGBA(top[0], top[1], top[2], 1), RGBA(bot[0], bot[1], bot[2], 1));
    /* odbicie: szklany połysk na górnej połowie */
    MIFillGradient(c, CGRectMake(0, 0, w, h * 0.5f), RGBA(255, 255, 255, 0.55f), RGBA(255, 255, 255, 0.10f));
    CGContextRestoreGState(c);
    [RGBA(border[0], border[1], border[2], 1) setStroke];
    shape.lineWidth = 1.0f;
    [shape stroke];
    /* lekki cień pod spodem */
    [RGBA(255, 255, 255, 0.18f) setFill];
    CGContextFillRect(c, CGRectMake(6, h - 1, w - 12, 1));
    UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    gGelCache[th][style] = [img resizableImageWithCapInsets:UIEdgeInsetsMake(14, 10, 14, 10)];
    return gGelCache[th][style];
}

/* mała próbka koloru motywu do listy w ustawieniach */
UIImage *MIThemeSwatch(NSInteger index)
{
    MIPalette p;
    if (index < 0 || index >= NTHEMES) index = 0;
    BuildPalette(index, &p);
    CGFloat s = 30.0f;
    UIGraphicsBeginImageContextWithOptions(CGSizeMake(s, s), NO, 0.0f);
    CGContextRef c = UIGraphicsGetCurrentContext();
    CGRect r = CGRectMake(1.5f, 1.5f, s - 3.0f, s - 3.0f);
    UIBezierPath *shape = [UIBezierPath bezierPathWithRoundedRect:r cornerRadius:8.0f];
    CGContextSaveGState(c);
    [shape addClip];
    MIFillGradient(c, CGRectMake(0, 0, s, s), MIRGB(p.accentGelT, 1), MIRGB(p.accentGelB, 1));
    MIFillGradient(c, CGRectMake(0, 0, s, s * 0.5f), RGBA(255, 255, 255, 0.55f), RGBA(255, 255, 255, 0.10f));
    CGContextRestoreGState(c);
    [MIRGB(p.accentGelBorder, 1) setStroke];
    shape.lineWidth = 1.0f;
    [shape stroke];
    UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return img;
}

UIButton *MIGelButton(NSString *title, int style, CGRect frame)
{
    UIButton *b = [UIButton buttonWithType:UIButtonTypeCustom];
    b.frame = frame;
    [b setBackgroundImage:MIGelImage(style) forState:UIControlStateNormal];
    [b setTitle:title forState:UIControlStateNormal];
    [b setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    [b setTitleShadowColor:[UIColor colorWithWhite:0.0f alpha:0.6f] forState:UIControlStateNormal];
    b.titleLabel.shadowOffset = CGSizeMake(0, -1);
    b.titleLabel.font = [UIFont boldSystemFontOfSize:17.0f];
    b.titleLabel.adjustsFontSizeToFitWidth = YES;
    b.titleLabel.minimumScaleFactor = 0.65f;
    b.adjustsImageWhenHighlighted = YES;
    return b;
}

UIBarButtonItem *MIBarButton(NSString *title, id target, SEL action)
{
    return [[UIBarButtonItem alloc] initWithTitle:title style:UIBarButtonItemStyleBordered target:target action:action];
}

/* ---------- daty ---------- */

static NSString *MIFormatted(double ts, int kind)
{
    static NSDateFormatter *cache[3][2];
    NSInteger l = MILanguage();
    if (!cache[l][kind]) {
        NSDateFormatter *f = [[NSDateFormatter alloc] init];
        f.locale = MILocale();
        f.dateFormat = kind == 0 ? @"d MMM yyyy" : @"d MMM yyyy, HH:mm";
        cache[l][kind] = f;
    }
    return [cache[l][kind] stringFromDate:[NSDate dateWithTimeIntervalSince1970:ts]];
}

NSString *MIDateString(double ts)     { return MIFormatted(ts, 0); }
NSString *MIDateTimeString(double ts) { return MIFormatted(ts, 1); }
