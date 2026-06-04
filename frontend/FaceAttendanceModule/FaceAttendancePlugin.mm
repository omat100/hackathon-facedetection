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

+ (BOOL)requiresMainQueueSetup {
    return YES;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        @try {
            // ── Resolve model paths from the app bundle ───────────────────
            NSString* yunetPath = [[NSBundle mainBundle]
                pathForResource:@"face_detection_yunet_2023mar" ofType:@"onnx"];
            NSString* sfacePath = [[NSBundle mainBundle]
                pathForResource:@"face_recognition_sface_2021dec" ofType:@"onnx"];

            if (!yunetPath || !sfacePath) {
                RCTLogError(@"[FaceAttendanceModule] ONNX models not found in bundle! "
                            @"Make sure both .onnx files are added to Copy Bundle Resources.");
                return self;
            }

            // ── Use Documents dir for writable DB / face JSON ─────────────
            NSString* docsDir = NSSearchPathForDirectoriesInDomains(
                NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
            NSString* dbPath   = [docsDir stringByAppendingPathComponent:@"attendance.db"];
            NSString* jsonPath = [docsDir stringByAppendingPathComponent:@"face_database.json"];

            RCTLogInfo(@"[FaceAttendanceModule] YuNet  : %@", yunetPath);
            RCTLogInfo(@"[FaceAttendanceModule] SFace  : %@", sfacePath);
            RCTLogInfo(@"[FaceAttendanceModule] DB     : %@", dbPath);
            RCTLogInfo(@"[FaceAttendanceModule] FaceDB : %@", jsonPath);

            // ── Construct C++ engines ─────────────────────────────────────
            _faceEngine = new FaceRecognitionEngine(
                [yunetPath UTF8String],
                [sfacePath UTF8String],
                [jsonPath  UTF8String]
            );
            _liveness = new LivenessDetector([yunetPath UTF8String]);
            _storage  = new AttendanceStorage([dbPath UTF8String]);
            _liveness->start();

            RCTLogInfo(@"[FaceAttendanceModule] C++ engines initialized successfully");
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

// ─── initializeModule ────────────────────────────────────────────────────────

RCT_EXPORT_METHOD(initializeModule:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    if (_faceEngine && _liveness && _storage) {
        resolve(@(YES));
    } else {
        reject(@"init_error", @"C++ engines failed to initialize", nil);
    }
}

// ─── processBase64 ───────────────────────────────────────────────────────────

RCT_EXPORT_METHOD(processBase64:(NSString*)base64
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    if (!base64 || base64.length == 0) {
        reject(@"invalid_input", @"base64 string is empty", nil);
        return;
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @try {
            NSData* data = [[NSData alloc] initWithBase64EncodedString:base64
                            options:NSDataBase64DecodingIgnoreUnknownCharacters];
            if (!data || data.length == 0) {
                reject(@"decode_error", @"Failed to decode base64", nil);
                return;
            }

            std::vector<uchar> buf((uchar*)data.bytes, (uchar*)data.bytes + data.length);
            cv::Mat frame = cv::imdecode(buf, cv::IMREAD_COLOR);
            if (frame.empty()) {
                reject(@"decode_error", @"Failed to decode image from base64", nil);
                return;
            }

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
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    if (!imagePath) {
        reject(@"invalid_path", @"Image path is null", nil);
        return;
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @try {
            cv::Mat frame = cv::imread([imagePath UTF8String]);
            if (frame.empty()) {
                reject(@"read_error", @"Failed to read image from path", nil);
                return;
            }
            [self processFrame:frame resolver:resolve rejecter:reject];
        } @catch (NSException* e) {
            dispatch_async(dispatch_get_main_queue(), ^{
                reject(@"processing_error", e.reason, nil);
            });
        }
    });
}

// ─── Shared frame processing logic ───────────────────────────────────────────

- (void)processFrame:(cv::Mat)frame
            resolver:(RCTPromiseResolveBlock)resolve
            rejecter:(RCTPromiseRejectBlock)reject
{
    auto [livenessState, livenessMsg, landmarks] = _liveness->process_frame(frame);

    BOOL livenessVerified = (livenessState == LivenessState::VERIFIED);
    BOOL livenessFailed   = (livenessState == LivenessState::FAILED);

    std::string personId;
    double confidence = 0.0;

    if (livenessVerified) {
        auto rec_result = _faceEngine->recognize_face(frame);
        personId   = std::get<0>(rec_result);
        confidence = std::get<1>(rec_result);
        if (!personId.empty()) {
            _storage->log(personId, confidence);
        }
    }

    NSMutableDictionary* result = [NSMutableDictionary dictionary];
    result[@"liveness_verified"] = @(livenessVerified);
    result[@"liveness_failed"]   = @(livenessFailed);
    result[@"liveness_message"]  = [NSString stringWithUTF8String:livenessMsg.c_str()];
    result[@"person_id"]         = personId.empty()
        ? @""
        : [NSString stringWithUTF8String:personId.c_str()];
    result[@"confidence"]        = @(confidence);
    result[@"recognized"]        = @(!personId.empty());

    dispatch_async(dispatch_get_main_queue(), ^{
        resolve(result);
    });
}

// ─── registerFace ────────────────────────────────────────────────────────────

RCT_EXPORT_METHOD(registerFace:(NSString*)base64
                  personId:(NSString*)personId
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    if (!base64 || base64.length == 0 || !personId || personId.length == 0) {
        reject(@"invalid_input", @"base64 or personId is empty", nil);
        return;
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @try {
            NSData* data = [[NSData alloc] initWithBase64EncodedString:base64
                            options:NSDataBase64DecodingIgnoreUnknownCharacters];
            if (!data || data.length == 0) {
                reject(@"decode_error", @"Failed to decode base64", nil);
                return;
            }

            std::vector<uchar> buf((uchar*)data.bytes, (uchar*)data.bytes + data.length);
            cv::Mat frame = cv::imdecode(buf, cv::IMREAD_COLOR);
            if (frame.empty()) {
                reject(@"decode_error", @"Failed to decode image", nil);
                return;
            }

            std::string pid = [personId UTF8String];
            auto reg_result = _faceEngine->register_face(frame, pid);
            BOOL reg_success = (BOOL)reg_result.first;
            NSString* message = [NSString stringWithUTF8String:reg_result.second.c_str()];

            dispatch_async(dispatch_get_main_queue(), ^{
                resolve(@{
                    @"success": @(reg_success),
                    @"message": message
                });
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
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    if (_liveness) {
        _liveness->start();
        resolve(@(YES));
    } else {
        reject(@"not_initialized", @"Liveness detector not initialized", nil);
    }
}

@end
