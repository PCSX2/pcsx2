// PlaySoundAsync.mm — achievement sound playback.

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

// AVAudioPlayer.delegate is weak and the player is not otherwise retained, so the
// delegate strongly owns its player for the duration of playback.
@interface ARMSX2AchievementSoundDelegate : NSObject <AVAudioPlayerDelegate>
@property(nonatomic, strong) AVAudioPlayer* player;
@end

static NSMutableSet<ARMSX2AchievementSoundDelegate*>* ARMSX2ActiveAchievementSoundDelegates() {
    static NSMutableSet* set;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ set = [[NSMutableSet alloc] init]; });
    return set;
}

static dispatch_queue_t ARMSX2AchievementSoundQueue() {
    static dispatch_queue_t queue;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        queue = dispatch_queue_create("armsx2.achievement.sound", DISPATCH_QUEUE_SERIAL);
    });
    return queue;
}

@implementation ARMSX2AchievementSoundDelegate
- (void)audioPlayerDidFinishPlaying:(AVAudioPlayer*)player successfully:(BOOL)flag {
    self.player = nil;
    // The active-delegate set is mutated exclusively from this serial queue, so add
    // and remove hop on via dispatch_async instead of taking a lock.
    dispatch_async(ARMSX2AchievementSoundQueue(), ^{
        [ARMSX2ActiveAchievementSoundDelegates() removeObject:player.delegate];
    });
}
@end

#pragma mark - PlaySoundAsync
namespace Common {
bool PlaySoundAsync(const char* path) {
    if (!path || path[0] == '\0')
        return false;

    NSString* nspath = [[NSString alloc] initWithUTF8String:path];
    if (![[NSFileManager defaultManager] fileExistsAtPath:nspath])
        return false;

    NSURL* url = [NSURL fileURLWithPath:nspath];
    dispatch_async(ARMSX2AchievementSoundQueue(), ^{
        NSError* error = nil;
        AVAudioPlayer* player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];
        if (!player || error) {
            return;
        }
        ARMSX2AchievementSoundDelegate* delegate = [[ARMSX2AchievementSoundDelegate alloc] init];
        player.delegate = delegate;
        delegate.player = player;
        [ARMSX2ActiveAchievementSoundDelegates() addObject:delegate];
        [player prepareToPlay];
        [player play];
    });
    return true;
}
}
