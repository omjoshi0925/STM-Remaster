// AudioManager.h - plays the original sound set by event name.
#pragma once
#import <Foundation/Foundation.h>
#include <string>

@interface TMAudioManager : NSObject
// assetRoot is the Assets directory; the manager reads configs/VoxSounds.bin
// and streams clips from sounds/. Returns nil only if the table is missing.
- (instancetype)initWithAssetRoot:(const std::string &)assetRoot;
- (void)playEvent:(const char *)event;              // fire and forget (sfx)
- (void)playMusic:(const char *)event looping:(BOOL)loop;
- (void)stopMusic;
- (void)setMuted:(BOOL)muted;
@property (nonatomic, readonly) NSUInteger eventCount;
@end
