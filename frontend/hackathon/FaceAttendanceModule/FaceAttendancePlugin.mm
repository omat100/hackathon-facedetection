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
            NSString* yunetPath = [[NSBundle mainBundle]
                pathForResource:@"face_detection_yunet_2023mar" ofType:@"onnx"];
            // CHANGED: use w600k_mbf (buffalo_sc) instead of SFace
            NSString* recPath = [[NSBundle mainBundle]
                pathForResource:@"w600k_mbf" ofType:@"onnx"];

            if (!yunetPath || !recPath) {
                RCTLogError(@"[FaceAttendanceModule] ONNX models not found in bundle! "
                            @"Add face_detection_yunet_2023mar.onnx and w600k_mbf.onnx "
                            @"to Copy Bundle Resources in Xcode.");
                return self;
            }

            NSString* docsDir = NSSearchPathForDirectoriesInDomains(
                NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
            NSString* dbPath   = [docsDir stringByAppendingPathComponent:@"attendance.db"];
            NSString* jsonPath = [docsDir stringByAppendingPathComponent:@"face_database.json"];

            NSString* bundledJSON = [[NSBundle mainBundle]
                pathForResource:@"face_database" ofType:@"json"];
            NSFileManager* fm = [NSFileManager defaultManager];
            if (bundledJSON && ![fm fileExistsAtPath:jsonPath]) {
                NSError* err = nil;
                [fm copyItemAtPath:bundledJSON toPath:jsonPath error:&err];
                if (err) {
                    RCTLogError(@"[FaceAttendanceModule] Failed to copy face_database.json: %@", err);
                } else {
                    RCTLogInfo(@"[FaceAttendanceModule] Copied bundled face_database.json to Documents");
                }
            }

            RCTLogInfo(@"[FaceAttendanceModule] YuNet  : %@", yunetPath);
            RCTLogInfo(@"[FaceAttendanceModule] RecNet : %@", recPath);
            RCTLogInfo(@"[FaceAttendanceModule] DB     : %@", dbPath);
            RCTLogInfo(@"[FaceAttendanceModule] FaceDB : %@", jsonPath);

            _faceEngine = new FaceRecognitionEngine(
                [yunetPath UTF8String],
                [recPath   UTF8String],
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

- (BOOL)enginesReady:(RCTPromiseRejectBlock)reject {
    if (!_faceEngine || !_liveness || !_storage) {
        reject(@"not_initialized",
               @"C++ engines not initialized. "
               @"Check that both .onnx files are in Copy Bundle Resources.",
               nil);
        return NO;
    }
    return YES;
}

RCT_EXPORT_METHOD(initializeModule:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    if (_faceEngine && _liveness && _storage) {
        resolve(@(YES));
    } else {
        reject(@"init_error",
               @"C++ engines failed to initialize. "
               @"Make sure face_detection_yunet_2023mar.onnx and "
               @"w600k_mbf.onnx are in Copy Bundle Resources.",
               nil);
    }
}

RCT_EXPORT_METHOD(processBase64:(NSString*)base64
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    if (![self enginesReady:reject]) return;

    if (!base64 || base64.length == 0) {
        reject(@"invalid_input", @"base64 string is empty", nil);
        return;
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @try {
            NSString* clean = base64;
            NSRange commaRange = [base64 rangeOfString:@","];
            if (commaRange.location != NSNotFound && [base64 hasPrefix:@"data:"]) {
                clean = [base64 substringFromIndex:commaRange.location + 1];
            }

            NSData* data = [[NSData alloc] initWithBase64EncodedString:clean
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

RCT_EXPORT_METHOD(processFaceImage:(NSString*)imagePath
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    if (![self enginesReady:reject]) return;

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
        // Use threshold 0.4 — same as Python buffalo_sc version
        auto rec_result = _faceEngine->recognize_face(frame, 0.4);
        personId   = std::get<0>(rec_result);
        confidence = std::get<1>(rec_result);

        if (!personId.empty() && confidence >= 0.4) {
            _storage->log(personId, confidence);
        } else {
            personId  = "";
            confidence = 0.0;
        }
        _liveness->start();
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

RCT_EXPORT_METHOD(registerFace:(NSString*)base64
                  personId:(NSString*)personId
                  resolver:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    if (![self enginesReady:reject]) return;

    if (!base64 || base64.length == 0 || !personId || personId.length == 0) {
        reject(@"invalid_input", @"base64 or personId is empty", nil);
        return;
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @try {
            NSString* clean = base64;
            NSRange commaRange = [base64 rangeOfString:@","];
            if (commaRange.location != NSNotFound && [base64 hasPrefix:@"data:"]) {
                clean = [base64 substringFromIndex:commaRange.location + 1];
            }

            NSData* data = [[NSData alloc] initWithBase64EncodedString:clean
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

RCT_EXPORT_METHOD(resetLiveness:(RCTPromiseResolveBlock)resolve
                  rejecter:(RCTPromiseRejectBlock)reject)
{
    if (!_liveness) {
        reject(@"not_initialized", @"Liveness detector not initialized", nil);
        return;
    }
    _liveness->start();
    resolve(@(YES));
}

@end
