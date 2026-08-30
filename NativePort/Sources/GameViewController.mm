#import "GameViewController.h"
#import <MetalKit/MetalKit.h>
#import <CoreMotion/CoreMotion.h>
#import "Renderer.h"
@interface GameViewController(){ MTKView *_view; TMRenderer *_renderer; CMMotionManager *_motion; UILabel *_status; }
@end
@implementation GameViewController
- (void)loadView { _view=[[MTKView alloc] initWithFrame:UIScreen.mainScreen.bounds]; self.view=_view; }
- (void)viewDidLoad { [super viewDidLoad]; self.view.multipleTouchEnabled=YES;
 _status=[[UILabel alloc] init]; _status.translatesAutoresizingMaskIntoConstraints=NO; _status.textColor=UIColor.whiteColor; _status.numberOfLines=0; _status.font=[UIFont monospacedSystemFontOfSize:12 weight:UIFontWeightRegular]; _status.backgroundColor=[UIColor colorWithWhite:0 alpha:.45]; [self.view addSubview:_status];
 [NSLayoutConstraint activateConstraints:@[[_status.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:12],[_status.trailingAnchor constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-12],[_status.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:12]]];
 _renderer=[[TMRenderer alloc] initWithView:_view statusLabel:_status];
 _motion=[[CMMotionManager alloc] init]; if(_motion.accelerometerAvailable){_motion.accelerometerUpdateInterval=1.0/60.0; __weak TMRenderer *r=_renderer; [_motion startAccelerometerUpdatesToQueue:NSOperationQueue.mainQueue withHandler:^(CMAccelerometerData*d,NSError*e){ if(d && !e)[r accelerometerX:d.acceleration.x y:d.acceleration.y z:d.acceleration.z]; }];}
}
- (CGPoint)point:(NSSet<UITouch*>*)touches { UITouch*t=touches.anyObject; return [t locationInView:self.view]; }
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [_renderer touchBegin:[self point:touches]]; }
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [_renderer touchMove:[self point:touches]]; }
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [_renderer touchEnd:[self point:touches]]; }
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [_renderer touchEnd:[self point:touches]]; }
- (BOOL)prefersHomeIndicatorAutoHidden{return YES;} - (BOOL)prefersStatusBarHidden{return YES;} - (UIInterfaceOrientationMask)supportedInterfaceOrientations{return UIInterfaceOrientationMaskLandscape;}
@end
