#import <UIKit/UIKit.h>

/* Wspólne widoki: komórki z połyskiem, nagłówki sekcji, wykres, pole liczby. */

@interface MICell : UITableViewCell          /* podtytuł + błyszczące tło */
+ (MICell *)cellForTable:(UITableView *)table identifier:(NSString *)ident;
@end

@interface MISectionHeader : UIView
- (id)initWithTitle:(NSString *)title right:(NSString *)right;
@end

UIView *MIHeaderForTitle(NSString *title, NSString *right);

UIToolbar *MIKeyboardToolbar(id target, SEL doneAction);
UIImage *MICheckImage(BOOL on);
/* Otwiera adres w przeglądarce. Klasa NSURL jest wyszukiwana dopiero w czasie działania (patrz uwaga w README). */
void MIOpenURL(NSString *url);

/* Obiekt NSURL zbudowany dopiero w czasie działania (klasy sieciowe wyszukujemy po nazwie, patrz README). */
id MIURL(NSString *string);
id MIFileURL(NSString *path);
/* "3,4 MB" */
NSString *MIBytesString(double bytes);
/* "3:25" */
NSString *MIDurationString(long ms);

/* Proste okienko z komunikatem. */
void MIAlert(NSString *title, NSString *message);
/* Kolor tekstu błędu (czerwony, dobrany do jasności motywu) */
UIColor *MIErrorColor(void);
