#import <UIKit/UIKit.h>

/* Edycja tagów jednego utworu z kolejki (tytuł, wykonawca, album...). */
@interface MIEditController : UIViewController <UITableViewDataSource, UITableViewDelegate, UITextFieldDelegate, UIActionSheetDelegate>
- (id)initWithItemId:(NSString *)iid;
@end
