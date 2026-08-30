#import "AppDelegate.h"
#import "GameViewController.h"
@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  self.window=[[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
  self.window.rootViewController=[[GameViewController alloc] init];
  [self.window makeKeyAndVisible];
  return YES;
}
- (void)applicationWillResignActive:(UIApplication *)application { [[NSNotificationCenter defaultCenter] postNotificationName:@"TMWillPause" object:nil]; }
- (void)applicationDidBecomeActive:(UIApplication *)application { [[NSNotificationCenter defaultCenter] postNotificationName:@"TMDidResume" object:nil]; }
@end
