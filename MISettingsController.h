#import <UIKit/UIKit.h>

@interface MISettingsController : UIViewController <UITableViewDataSource, UITableViewDelegate, UIActionSheetDelegate>
@end

/* Lista do wyboru jednej opcji (język, motyw) z ptaszkiem przy wybranej. */
@interface MIChoiceController : UIViewController <UITableViewDataSource, UITableViewDelegate>
@property (nonatomic, copy) NSArray *titles;
@property (nonatomic, copy) NSArray *images;                /* opcjonalnie: UIImage na każdy wiersz */
@property (nonatomic) NSInteger selected;
@property (nonatomic, copy) void (^onChoose)(NSInteger index);
@end
