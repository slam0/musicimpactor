#import <UIKit/UIKit.h>

/* Główny ekran: kolejka utworów do dodania do Muzyki (pobieranie z linku, pliki z "Otwórz w...", pliki z iTunes). */
@interface MIQueueController : UIViewController <UITableViewDataSource, UITableViewDelegate, UIActionSheetDelegate, UIAlertViewDelegate>
- (void)reloadSoon;
@end
