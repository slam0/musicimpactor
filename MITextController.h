#import <UIKit/UIKit.h>

/* Pełnoekranowy tekst do przeczytania i skopiowania (raport z importu, diagnostyka). */
@interface MITextController : UIViewController
- (id)initWithTitle:(NSString *)title text:(NSString *)text;
/* Pokazuje kontroler w oknie modalnym z paskiem nawigacji. */
+ (void)presentTitle:(NSString *)title text:(NSString *)text from:(UIViewController *)from;
@end
