#import <UIKit/UIKit.h>

/* Wygląd w stylu iOS 6 ("grafit z połyskiem") z kilkoma motywami kolorystycznymi
   oraz drobne funkcje pomocnicze do formatowania liczb, dat i jednostek. */

/* ---- paleta bieżącego motywu ---- */
typedef struct {
    int accent[3], accentGelT[3], accentGelB[3], accentGelBorder[3];
    int bg[3], bgHi[3], bgLo[3];
    int barT1[3], barT2[3], barB1[3], barB2[3], barTint[3];
    int cellT1[3], cellT2[3], cellB1[3], cellB2[3];
    int selT1[3], selT2[3], selB1[3], selB2[3];
    int text[3], subtle[3], tabOff[3], tabOn[3];
    int hdrT[3], hdrB[3], hdrTitle[3], hdrRight[3];
    int fieldT[3], fieldB[3], fieldText[3];
    int tileT[3], tileB[3], tileText[3];
    int panelT[3], panelB[3], grid[3];
    int sep[3], neutT[3], neutB[3], neutBorder[3];
    int shadow[3], chrome[3], danger[3];
    float gridAlpha, shadowAlpha, shadowDY, cellHiAlpha, cellLoAlpha;
    int light;
} MIPalette;

const MIPalette *MIP(void);
UIColor *MIRGB(const int *rgb, CGFloat alpha);

/* ---- motywy ---- */
NSInteger MIThemeCount(void);
NSInteger MICurrentTheme(void);
void MISetTheme(NSInteger index);               /* zapisuje i ogłasza MIAppearanceDidChangeNotification */
NSString *MIThemeName(NSInteger index);         /* przetłumaczona nazwa */
UIImage *MIThemeSwatch(NSInteger index);        /* mała błyszcząca próbka koloru */

UIColor *MIBackgroundColor(void);       /* teksturowane tło tabel */
UIColor *MITextColor(void);
UIColor *MISubtleColor(void);
UIColor *MIAccentColor(void);
UIColor *MIGreenColor(void);
void MIStyleShadow(UILabel *label);     /* cień tekstu pasujący do motywu */

void MIStyleNavigationBar(UINavigationBar *bar);
void MIStyleTabBar(UITabBar *bar);
void MIStyleTabItem(UITabBarItem *item, int kind);   /* 0 kolejka, 1 zaimportowane, 3 ustawienia */
void MIStyleSearchBar(UISearchBar *bar);
UIImage *MITabIcon(int kind);
void MIStyleTable(UITableView *table);

/* styl: 0 kolor motywu, 1 neutralny, 2 czerwony, 3 zielony */
UIImage *MIGelImage(int style);
UIButton *MIGelButton(NSString *title, int style, CGRect frame);
UIBarButtonItem *MIBarButton(NSString *title, id target, SEL action);

/* daty */
NSString *MIDateString(double ts);                   /* 21 paź 2026 */
NSString *MIDateTimeString(double ts);
