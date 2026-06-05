#import <React/RCTBridgeModule.h>
#import <React/RCTLog.h>

#include <opencv2/imgcodecs.hpp>
#include "cpp/face_recognition.hpp"
#include "cpp/liveness_detector.hpp"
#include "cpp/storage.hpp"

@interface FaceAttendancePlugin : NSObject <RCTBridgeModule>
@end

@interface FaceAttendancePlugin () {
    FaceRecognitionEngine* _faceEngine;
    LivenessDetector*      _liveness;
    AttendanceStorage*     _storage;
}
@end

@implementation FaceAttendancePlugin

RCT_EXPORT_MODULE(FaceAttendanceModule);

+ (BOOL)requiresMainQueueSetup { return YES; }

- (instancetype)init {
    self = [super init];
    if (self) {
        @try {
            NSString* yunetPath = [[NSBundle mainBundle]
                pathForResource:@"face_detection_yunet_2023mar" ofType:@"onnx"];
            NSString* recogPath = [[NSBundle mainBundle]
                pathForResource:@"w600k_mbf" ofType:@"onnx"];

            if (!recogPath) {
                RCTLogWarn(@"[FaceAttendanceModule] w600k_mbf.onnx not found, falling back to SFace");
                recogPath = [[NSBundle mainBundle]
                    pathForResource:@"face_recognition_sface_2021dec" ofType:@"onnx"];
            }

            if (!yunetPath || !recogPath) {
                RCTLogError(@"[FaceAttendanceModule] ONNX models not found in bundle!");
                return self;
            }

            NSString* docsDir = NSSearchPathForDirectoriesInDomains(
                NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
            NSString* dbPath   = [docsDir stringByAppendingPathComponent:@"attendance.db"];
            NSString* jsonPath = [docsDir stringByAppendingPathComponent:@"face_database.json"];

            // Copy bundled face_database.json to Documents on first launch
            NSString* bundledJSON = [[NSBundle mainBundle]
                pathForResource:@"face_database" ofType:@"json"];
            NSFileManager* fm = [NSFileManager defaultManager];
            if (bundledJSON && ![fm fileExistsAtPath:jsonPath]) {
                NSError* err = nil;
                [fm copyItemAtPath:bundledJSON toPath:jsonPath error:&err];
            }

            RCTLogInfo(@"[FaceAttendanceModule] YuNet : %@", yunetPath);
            RCTLogInfo(@"[FaceAttendanceModule] Recog : %@", recogPath);
            RCTLogInfo(@"[FaceAttendanceModule] DB    : %@", dbPath);

            _faceEngine = new FaceRecognitionEngine(
                [yunetPath UTF8String], [recogPath UTF8String], [jsonPath UTF8String]);
            _liveness = new LivenessDetector([yunetPath UTF8String]);
            _storage  = new AttendanceStorage([dbPath UTF8String]);
            _liveness->start();

            RCTLogInfo(@"[FaceAttendanceModule] All C++ engines ready");
        } @catch (NSException* e) {
            RCTLogError(@"[FaceAttendanceModule] Init failed: %@", e.reason);
        }
    }
    return self;
}

- (void)dealloc {
    delete _faceEngine;
    delete _liveness;
    delete _storage;
}

// ─── Fix frame from iOS camera ───────────────────────────────────────────────
// Front camera gives BGRA or mirrored BGR. Fix both issues consistently
// so register and recognize always see the same orientation/colorspace.

- (cv::Mat)fixFrame:(cv::Mat)frame {
    // Convert BGRA -> BGR if needed
    if (frame.channels() == 4) {
        cv::cvtColor(frame, frame, cv::COLOR_BGRA2BGR);
    }
    // Front camera on iOS is mirrored — flip horizontally to match real world
    cv::flip(frame, frame, 1);
    return frame;
}

// ─── initializeModule ────────────────────────────────────────────────────────

RCT_EXPORT_METHOD(initializeModule:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {
    if (_faceEngine && _liveness && _storage)
        resolve(@(YES));
    else
        reject(@"init_error", @"C++ engines failed to initialize", nil);
}

// ─── Shared frame processor ──────────────────────────────────────────────────

- (void)processFrame:(cv::Mat)frame
            resolver:(RCTPromiseResolveBlock)resolve
            rejecter:(RCTPromiseRejectBlock)reject {

    auto liveness_result = _liveness->process_frame(frame);
    LivenessState livenessState = std::get<0>(liveness_result);
    std::string   livenessMsg   = std::get<1>(liveness_result);

    BOOL livenessVerified = (livenessState == LivenessState::VERIFIED);
    BOOL livenessFailed   = (livenessState == LivenessState::FAILED);

    std::string personId;
    double confidence = 0.0;
    std::string recogMsg;

    if (livenessVerified) {
        auto rec_result = _faceEngine->recognize_face(frame, 0.30);
        personId   = std::get<0>(rec_result);
        confidence = std::get<1>(rec_result);
        recogMsg   = std::get<2>(rec_result);

        if (!personId.empty() && confidence >= 0.30) {
            _storage->log(personId, confidence);
        } else {
            personId  = "";
            confidence = 0.0;
        }
        // Auto-reset liveness for next person
        _liveness->start();
    }

    NSMutableDictionary* result = [NSMutableDictionary dictionary];
    result[@"liveness_verified"] = @(livenessVerified);
    result[@"liveness_failed"]   = @(livenessFailed);
    result[@"liveness_message"]  = [NSString stringWithUTF8String:livenessMsg.c_str()];
    result[@"person_id"]         = personId.empty() ? @"" : [NSString stringWithUTF8String:personId.c_str()];
    result[@"confidence"]        = @(confidence);
    result[@"recognized"]        = @(!personId.empty());
    result[@"recog_message"]     = [NSString stringWithUTF8String:recogMsg.c_str()];

    dispatch_async(dispatch_get_main_queue(), ^{
        resolve(result);
    });
}

// ─── processBase64 ───────────────────────────────────────────────────────────

RCT_EXPORT_METHOD(processBase64:(NSString*)base64
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {

    if (!base64 || base64.length == 0) {
        reject(@"invalid_input", @"base64 string is empty", nil); return;
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @try {
            // Strip data URI prefix if present
            NSString* clean = base64;
            NSRange commaRange = [base64 rangeOfString:@","];
            if (commaRange.location != NSNotFound && [base64 hasPrefix:@"data:"]) {
                clean = [base64 substringFromIndex:commaRange.location + 1];
            }

            NSData* data = [[NSData alloc] initWithBase64EncodedString:clean
                            options:NSDataBase64DecodingIgnoreUnknownCharacters];
            if (!data || data.length == 0) {
                reject(@"decode_error", @"Failed to decode base64", nil); return;
            }
            std::vector<uchar> buf((uchar*)data.bytes, (uchar*)data.bytes + data.length);
            cv::Mat frame = cv::imdecode(buf, cv::IMREAD_COLOR);
            if (frame.empty()) {
                reject(@"decode_error", @"Failed to decode image", nil); return;
            }

            // FIX: normalize color format and orientation from iOS front camera
            frame = [self fixFrame:frame];

            [self processFrame:frame resolver:resolve rejecter:reject];
        } @catch (NSException* e) {
            dispatch_async(dispatch_get_main_queue(), ^{
                reject(@"processing_error", e.reason, nil);
            });
        }
    });
}

// ─── processFaceImage ────────────────────────────────────────────────────────

RCT_EXPORT_METHOD(processFaceImage:(NSString*)imagePath
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {

    if (!imagePath) {
        reject(@"invalid_path", @"Image path is null", nil); return;
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @try {
            cv::Mat frame = cv::imread([imagePath UTF8String]);
            if (frame.empty()) {
                reject(@"read_error", @"Failed to read image from path", nil); return;
            }
            // File-based images don't need flip/color fix
            [self processFrame:frame resolver:resolve rejecter:reject];
        } @catch (NSException* e) {
            dispatch_async(dispatch_get_main_queue(), ^{
                reject(@"processing_error", e.reason, nil);
            });
        }
    });
}

// ─── registerFace ────────────────────────────────────────────────────────────

RCT_EXPORT_METHOD(registerFace:(NSString*)base64
                  personId:(NSString*)personId
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {

    if (!base64 || base64.length == 0 || !personId || personId.length == 0) {
        reject(@"invalid_input", @"base64 or personId is empty", nil); return;
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @try {
            // Strip data URI prefix if present
            NSString* clean = base64;
            NSRange commaRange = [base64 rangeOfString:@","];
            if (commaRange.location != NSNotFound && [base64 hasPrefix:@"data:"]) {
                clean = [base64 substringFromIndex:commaRange.location + 1];
            }

            NSData* data = [[NSData alloc] initWithBase64EncodedString:clean
                            options:NSDataBase64DecodingIgnoreUnknownCharacters];
            if (!data || data.length == 0) {
                reject(@"decode_error", @"Failed to decode base64", nil); return;
            }
            std::vector<uchar> buf((uchar*)data.bytes, (uchar*)data.bytes + data.length);
            cv::Mat frame = cv::imdecode(buf, cv::IMREAD_COLOR);
            if (frame.empty()) {
                reject(@"decode_error", @"Failed to decode image", nil); return;
            }

            // FIX: same normalization as processBase64 so embeddings are consistent
            frame = [self fixFrame:frame];

            std::string pid = [personId UTF8String];
            auto reg_result = _faceEngine->register_face(frame, pid);
            BOOL success = (BOOL)reg_result.first;
            NSString* msg = [NSString stringWithUTF8String:reg_result.second.c_str()];

            dispatch_async(dispatch_get_main_queue(), ^{
                resolve(@{ @"success": @(success), @"message": msg });
            });
        } @catch (NSException* e) {
            dispatch_async(dispatch_get_main_queue(), ^{
                reject(@"register_error", e.reason, nil);
            });
        }
    });
}

// ─── resetLiveness ───────────────────────────────────────────────────────────

RCT_EXPORT_METHOD(resetLiveness:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {
    if (_liveness) {
        _liveness->start();
        resolve(@(YES));
    } else {
        reject(@"not_initialized", @"Liveness detector not ready", nil);
    }
}

// ─── getAttendanceStats ──────────────────────────────────────────────────────

RCT_EXPORT_METHOD(getAttendanceStats:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject) {
    if (!_storage) {
        reject(@"not_initialized", @"Storage not ready", nil); return;
    }
    auto s = _storage->stats();
    resolve(@{
        @"total":    @(s.total),
        @"synced":   @(s.synced),
        @"unsynced": @(s.unsynced)
    });
}

@end