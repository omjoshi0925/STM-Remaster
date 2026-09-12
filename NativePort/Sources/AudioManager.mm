#import "AudioManager.h"
#import <AVFoundation/AVFoundation.h>
#include "Audio.hpp"
#include <map>
#include <memory>

// One AVAudioEngine: a looping music player plus a small pool of effect
// players so overlapping hits do not cut each other off. Clips are decoded
// from the original IMA ADPCM WAVs by Audio.cpp and cached as PCM buffers.
static const NSUInteger kSfxVoices = 8;

@implementation TMAudioManager {
    std::string _root;
    std::unique_ptr<bdae::VoxTable> _vox;
    AVAudioEngine *_engine;
    AVAudioPlayerNode *_musicNode;
    NSMutableArray<AVAudioPlayerNode *> *_sfxNodes;
    NSUInteger _sfxNext;
    NSMutableDictionary<NSString *, AVAudioPCMBuffer *> *_cache;
    BOOL _muted;
}

- (instancetype)initWithAssetRoot:(const std::string &)assetRoot {
    if (!(self = [super init])) return nil;
    _root = assetRoot;
    _vox = std::make_unique<bdae::VoxTable>();
    std::string err;
    if (!_vox->load(assetRoot + "/configs", err)) {
        NSLog(@"[TotalMayhem] audio: %s (extract configs.pack)", err.c_str());
        return self;   // usable, but silent
    }
    NSError *sessionErr = nil;
    [AVAudioSession.sharedInstance setCategory:AVAudioSessionCategoryAmbient error:&sessionErr];
    [AVAudioSession.sharedInstance setActive:YES error:&sessionErr];

    _engine = [AVAudioEngine new];
    _musicNode = [AVAudioPlayerNode new];
    [_engine attachNode:_musicNode];
    [_engine connect:_musicNode to:_engine.mainMixerNode format:nil];
    _sfxNodes = [NSMutableArray new];
    for (NSUInteger i = 0; i < kSfxVoices; ++i) {
        AVAudioPlayerNode *n = [AVAudioPlayerNode new];
        [_engine attachNode:n];
        [_engine connect:n to:_engine.mainMixerNode format:nil];
        [_sfxNodes addObject:n];
    }
    _cache = [NSMutableDictionary new];
    NSError *startErr = nil;
    if (![_engine startAndReturnError:&startErr])
        NSLog(@"[TotalMayhem] audio engine failed: %@", startErr);
    else
        NSLog(@"[TotalMayhem] audio ready: %zu events", _vox->byName.size());
    return self;
}

- (NSUInteger)eventCount { return _vox ? _vox->byName.size() : 0; }
- (void)setMuted:(BOOL)muted { _muted = muted; _engine.mainMixerNode.outputVolume = muted ? 0.0f : 1.0f; }

- (AVAudioPCMBuffer *)bufferForEvent:(const char *)event {
    if (!_vox || !_engine) return nil;
    NSString *key = [NSString stringWithUTF8String:event];
    AVAudioPCMBuffer *cached = _cache[key];
    if (cached) return cached;
    const bdae::VoxEvent *ev = _vox->find(event);
    if (!ev) { NSLog(@"[TotalMayhem] unknown sound event %s", event); return nil; }
    bdae::WavClip clip;
    std::string err;
    if (!clip.load(_root + "/sounds/" + ev->file, err)) {
        NSLog(@"[TotalMayhem] sound %s: %s", event, err.c_str());
        return nil;
    }
    AVAudioFormat *fmt = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                          sampleRate:clip.sampleRate
                                                            channels:clip.channels
                                                         interleaved:NO];
    AVAudioFrameCount frames = (AVAudioFrameCount)(clip.samples.size() / clip.channels);
    AVAudioPCMBuffer *buf = [[AVAudioPCMBuffer alloc] initWithPCMFormat:fmt frameCapacity:frames];
    if (!buf) return nil;
    buf.frameLength = frames;
    for (uint16_t c = 0; c < clip.channels; ++c) {
        float *dst = buf.floatChannelData[c];
        for (AVAudioFrameCount i = 0; i < frames; ++i)
            dst[i] = clip.samples[(size_t)i * clip.channels + c] / 32768.0f;
    }
    _cache[key] = buf;
    return buf;
}

- (void)playEvent:(const char *)event {
    if (_muted || !_engine.isRunning) return;
    AVAudioPCMBuffer *buf = [self bufferForEvent:event];
    if (!buf) return;
    AVAudioPlayerNode *node = _sfxNodes[_sfxNext % kSfxVoices];
    _sfxNext++;
    [node stop];
    [node scheduleBuffer:buf atTime:nil options:AVAudioPlayerNodeBufferInterrupts completionHandler:nil];
    [node play];
}

- (void)playMusic:(const char *)event looping:(BOOL)loop {
    if (!_engine.isRunning) return;
    AVAudioPCMBuffer *buf = [self bufferForEvent:event];
    if (!buf) return;
    [_musicNode stop];
    AVAudioPlayerNodeBufferOptions opts = loop ? AVAudioPlayerNodeBufferLoops : 0;
    [_musicNode scheduleBuffer:buf atTime:nil options:opts completionHandler:nil];
    _musicNode.volume = 0.55f;
    [_musicNode play];
}

- (void)stopMusic { [_musicNode stop]; }
@end
