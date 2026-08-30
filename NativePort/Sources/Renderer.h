#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>
@class UILabel;
@interface TMRenderer : NSObject<MTKViewDelegate>
- (instancetype)initWithView:(MTKView*)view statusLabel:(UILabel*)label;
- (void)touchBegin:(CGPoint)p; - (void)touchMove:(CGPoint)p; - (void)touchEnd:(CGPoint)p;
- (void)accelerometerX:(float)x y:(float)y z:(float)z;
@end
