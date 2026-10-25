#import "MIDownloads.h"
#import "MIStore.h"
#import "MILang.h"
#import "MICrash.h"

static NSString *const kUserAgent = @"Mozilla/5.0 (iPhone; CPU iPhone OS 6_1 like Mac OS X) AppleWebKit/536.26 (KHTML, like Gecko) Version/6.0 Mobile/10B143 Safari/8536.25";

@class MIDownloads;

@interface MIDownloadTask : NSObject
{
@public
    NSString *iid;
    id connection;
    NSFileHandle *handle;
    NSString *tmpPath;
    NSString *suggested;
    long long expected, received;
    NSTimeInterval lastNotify;
    BOOL finished;
    BOOL sawSSL;
    __weak MIDownloads *owner;
}
@end

@implementation MIDownloadTask
@end

@interface MIDownloads ()
{
    NSMutableDictionary *_tasks;               /* id elementu -> MIDownloadTask */
    UIBackgroundTaskIdentifier _bg;
}
- (void)taskDidFinish:(MIDownloadTask *)t error:(NSString *)err;
@end

@implementation MIDownloads

+ (MIDownloads *)shared
{
    static MIDownloads *s;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ s = [[MIDownloads alloc] init]; });
    return s;
}

- (id)init
{
    if ((self = [super init])) {
        _tasks = [NSMutableDictionary dictionary];
        _bg = UIBackgroundTaskInvalid;
    }
    return self;
}

- (NSUInteger)activeCount { return [_tasks count]; }

/* ---------- adres ---------- */

static NSString *NormalizeAddress(NSString *a)
{
    NSString *s = [a stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (![s length]) return nil;
    NSRange sc = [s rangeOfString:@"://"];
    if (sc.location == NSNotFound) s = [@"http://" stringByAppendingString:s];
    BOOL needsEscape = NO;
    for (NSUInteger i = 0; i < [s length]; i++) {
        unichar c = [s characterAtIndex:i];
        if (c <= 32 || c > 126) { needsEscape = YES; break; }
    }
    if (needsEscape && [s rangeOfString:@"%"].location == NSNotFound) {
        NSString *e = [s stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
        if (e) s = e;
    }
    return s;
}

static NSString *NameFromAddress(NSString *s)
{
    NSString *tail = s;
    NSRange q = [tail rangeOfString:@"?"];
    if (q.location != NSNotFound) tail = [tail substringToIndex:q.location];
    NSString *last = [tail lastPathComponent];
    NSString *dec = [last stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    if (dec) last = dec;
    if ([last length] > 120) last = [last substringFromIndex:[last length] - 120];
    return last ? last : @"";
}

/* ---------- start ---------- */

- (NSString *)startWithAddress:(NSString *)address error:(NSString **)error
{
    NSString *s = NormalizeAddress(address);
    NSString *low = [s lowercaseString];
    if (!s || !([low hasPrefix:@"http://"] || [low hasPrefix:@"https://"])) {
        if (error) *error = GL(@"To nie wygląda na adres internetowy (http:// albo https://)");
        return nil;
    }
    NSString *iid = [[MIStore shared] addDownloadingItemForURL:s name:NameFromAddress(s)];
    [self beginTaskForId:iid address:s];
    return iid;
}

- (void)beginTaskForId:(NSString *)iid address:(NSString *)s
{
    Class urlClass = NSClassFromString(@"NSURL");
    Class requestClass = NSClassFromString(@"NSMutableURLRequest");
    Class connClass = NSClassFromString(@"NSURLConnection");
    id url = urlClass ? [urlClass URLWithString:s] : nil;
    if (!url || !requestClass || !connClass) {
        [[MIStore shared] finishDownloadId:iid tempPath:nil suggestedName:nil error:@"Nie udało się utworzyć połączenia"];
        return;
    }
    id req = [requestClass requestWithURL:url cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:30.0];
    [req setValue:kUserAgent forHTTPHeaderField:@"User-Agent"];
    [req setValue:@"audio/*,*/*;q=0.8" forHTTPHeaderField:@"Accept"];

    MIDownloadTask *t = [[MIDownloadTask alloc] init];
    t->iid = iid;
    t->owner = self;
    t->expected = -1;
    t->tmpPath = [[[MIStore shared] queueDir] stringByAppendingPathComponent:[iid stringByAppendingString:@".part"]];
    [[NSFileManager defaultManager] removeItemAtPath:t->tmpPath error:NULL];
    if (![[NSFileManager defaultManager] createFileAtPath:t->tmpPath contents:nil attributes:nil]) {
        [[MIStore shared] finishDownloadId:iid tempPath:nil suggestedName:nil error:@"Nie mogę utworzyć pliku (brak miejsca?)"];
        return;
    }
    t->handle = [NSFileHandle fileHandleForWritingAtPath:t->tmpPath];
    id conn = [[connClass alloc] initWithRequest:req delegate:t startImmediately:NO];
    if (!conn) {
        [[MIStore shared] finishDownloadId:iid tempPath:t->tmpPath suggestedName:nil error:@"Nie udało się utworzyć połączenia"];
        return;
    }
    t->connection = conn;
    [_tasks setObject:t forKey:iid];
    [self beginBackground];
    [(NSURLConnection *)conn scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    MICrumb(@"pobieranie: start");
    [(NSURLConnection *)conn start];
}

- (void)beginBackground
{
    if (_bg != UIBackgroundTaskInvalid) return;
    _bg = [[UIApplication sharedApplication] beginBackgroundTaskWithExpirationHandler:^{
        /* system kończy czas w tle - pobieranie zostanie przerwane, wpis dostanie błąd przy następnym uruchomieniu */
        _bg = UIBackgroundTaskInvalid;
    }];
}

- (void)endBackgroundIfIdle
{
    if ([_tasks count] == 0 && _bg != UIBackgroundTaskInvalid) {
        [[UIApplication sharedApplication] endBackgroundTask:_bg];
        _bg = UIBackgroundTaskInvalid;
    }
}

- (void)cancelId:(NSString *)iid
{
    MIDownloadTask *t = [_tasks objectForKey:iid];
    if (t) {
        t->finished = YES;
        [(NSURLConnection *)t->connection cancel];
        [t->handle closeFile];
        [[NSFileManager defaultManager] removeItemAtPath:t->tmpPath error:NULL];
        [_tasks removeObjectForKey:iid];
        [self endBackgroundIfIdle];
    }
    [[MIStore shared] deleteItemId:iid];
}

- (void)retryId:(NSString *)iid
{
    NSDictionary *it = [[MIStore shared] itemWithId:iid];
    NSString *url = [it objectForKey:@"url"];
    if (!url || [_tasks objectForKey:iid]) return;
    [[MIStore shared] updateItemId:iid values:[NSDictionary dictionaryWithObjectsAndKeys:@"downloading", @"status", [NSNumber numberWithFloat:0.0f], @"progress", @"", @"error", nil] save:NO];
    [self beginTaskForId:iid address:url];
}

/* ---------- zakończenie ---------- */

- (void)taskDidFinish:(MIDownloadTask *)t error:(NSString *)err
{
    if (t->finished) return;
    t->finished = YES;
    [t->handle closeFile];
    t->handle = nil;
    [_tasks removeObjectForKey:t->iid];
    [self endBackgroundIfIdle];
    MICrumb(err ? @"pobieranie: błąd" : @"pobieranie: koniec");
    [[MIStore shared] finishDownloadId:t->iid tempPath:t->tmpPath suggestedName:t->suggested error:err];
}

@end

/* ---------- delegat połączenia (metody wywoływane przez NSURLConnection) ---------- */

@implementation MIDownloadTask (Delegate)

- (void)connection:(id)c didReceiveResponse:(NSURLResponse *)resp
{
    if (finished) return;
    expected = [resp expectedContentLength];
    NSString *name = [resp suggestedFilename];
    if ([name length]) suggested = name;
    if ([resp respondsToSelector:@selector(statusCode)]) {
        NSInteger code = [(NSHTTPURLResponse *)resp statusCode];
        if (code >= 400) {
            [(NSURLConnection *)c cancel];
            [owner taskDidFinish:self error:[NSString stringWithFormat:GL(@"Serwer odpowiedział błędem %d"), (int)code]];
            return;
        }
    }
    NSString *mime = [[resp MIMEType] lowercaseString];
    if ([mime hasPrefix:@"text/"]) {
        [(NSURLConnection *)c cancel];
        [owner taskDidFinish:self error:GL(@"To nie jest plik audio, tylko strona internetowa (sprawdź link)")];
        return;
    }
}

- (void)connection:(id)c didReceiveData:(NSData *)data
{
    if (finished) return;
    @try {
        [handle writeData:data];
    } @catch (NSException *e) {
        [(NSURLConnection *)c cancel];
        [owner taskDidFinish:self error:GL(@"Nie udało się zapisać pliku (brak miejsca?)")];
        return;
    }
    received += (long long)[data length];
    NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
    if (now - lastNotify > 0.3) {
        lastNotify = now;
        float p = expected > 0 ? (float)((double)received / (double)expected) : -1.0f;
        [[MIStore shared] updateItemId:iid values:[NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithFloat:p], @"progress", [NSNumber numberWithDouble:(double)received], @"received",
            [NSNumber numberWithDouble:(double)expected], @"expectedBytes", nil] save:NO];
    }
}

- (void)connectionDidFinishLoading:(id)c
{
    if (received == 0) { [owner taskDidFinish:self error:GL(@"Serwer nie przysłał żadnych danych")]; return; }
    [owner taskDidFinish:self error:nil];
}

- (void)connection:(id)c didFailWithError:(NSError *)error
{
    NSString *msg = GL(@"Nie udało się pobrać pliku");
    if ([[error domain] isEqualToString:@"NSURLErrorDomain"]) {
        NSInteger code = [error code];
        if (code == NSURLErrorNotConnectedToInternet || code == NSURLErrorDataNotAllowed || code == NSURLErrorInternationalRoamingOff)
            msg = GL(@"Brak połączenia z internetem");
        else if (code == NSURLErrorTimedOut) msg = GL(@"Serwer nie odpowiada (przekroczono czas)");
        else if (code == NSURLErrorCannotFindHost || code == NSURLErrorDNSLookupFailed) msg = GL(@"Nie znaleziono serwera (sprawdź adres)");
        else if (code <= NSURLErrorSecureConnectionFailed && code >= NSURLErrorClientCertificateRequired)
            msg = GL(@"Nie udało się połączyć bezpiecznie (HTTPS). Na iOS 6 część stron nie działa - spróbuj linku zaczynającego się od http://");
    }
    [owner taskDidFinish:self error:msg];
}

@end
