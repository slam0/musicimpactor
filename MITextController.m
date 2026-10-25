#import "MITextController.h"
#import "MITheme.h"
#import "MILang.h"
#import "MIViews.h"

@interface MITextController ()
{
    NSString *_text;
    UITextView *_view;
}
@end

@implementation MITextController

- (id)initWithTitle:(NSString *)title text:(NSString *)text
{
    if ((self = [super init])) {
        self.title = title;
        _text = [text copy];
    }
    return self;
}

+ (void)presentTitle:(NSString *)title text:(NSString *)text from:(UIViewController *)from
{
    MITextController *vc = [[MITextController alloc] initWithTitle:title text:text];
    UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:vc];
    MIStyleNavigationBar(nav.navigationBar);
    [from presentViewController:nav animated:YES completion:nil];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.backgroundColor = MIBackgroundColor();
    _view = [[UITextView alloc] initWithFrame:self.view.bounds];
    _view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _view.editable = NO;
    _view.font = [UIFont fontWithName:@"Courier" size:11.0f];
    _view.backgroundColor = [UIColor clearColor];
    _view.textColor = MITextColor();
    _view.text = _text;
    _view.alwaysBounceVertical = YES;
    [self.view addSubview:_view];
    self.navigationItem.leftBarButtonItem = MIBarButton(GL(@"Zamknij"), self, @selector(closeTapped));
    self.navigationItem.rightBarButtonItem = MIBarButton(GL(@"Kopiuj"), self, @selector(copyTapped));
}

- (void)closeTapped { [self dismissViewControllerAnimated:YES completion:nil]; }

- (void)copyTapped
{
    [UIPasteboard generalPasteboard].string = _text ? _text : @"";
    MIAlert(nil, GL(@"Skopiowano do schowka"));
}

@end
