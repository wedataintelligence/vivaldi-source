// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_stability_metrics_provider.h"

#include <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/metrics/metrics_service.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/crash_report/crash_report_background_uploader.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"

namespace {

// Logs |type| in the shutdown type histogram.
void LogShutdownType(MobileSessionShutdownType type) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION("Stability.MobileSessionShutdownType",
                                      type, MOBILE_SESSION_SHUTDOWN_TYPE_COUNT);
}

}  // namespace

IOSStabilityMetricsProvider::IOSStabilityMetricsProvider(
    metrics::MetricsService* metrics_service)
    : metrics_service_(metrics_service) {
  DCHECK(metrics_service_);
}

IOSStabilityMetricsProvider::~IOSStabilityMetricsProvider() {
}

bool IOSStabilityMetricsProvider::HasInitialStabilityMetrics() {
  return true;
}

void IOSStabilityMetricsProvider::ProvideInitialStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  // If this is the first launch after an upgrade, existing crash reports
  // may have been deleted before this code runs, so log this case in its
  // own bucket.
  if (IsFirstLaunchAfterUpgrade()) {
    LogShutdownType(FIRST_LAUNCH_AFTER_UPGRADE);
    return;
  }

  // If the last app lifetime did not end with a crash, then log it as a
  // normal shutdown while in the background.
  if (metrics_service_->WasLastShutdownClean()) {
    LogShutdownType(SHUTDOWN_IN_BACKGROUND);
    return;
  }

  // If the last app lifetime ended in a crash, log the type of crash.
  MobileSessionShutdownType shutdown_type;
  const bool with_crash_log =
      HasUploadedCrashReportsInBackground() || HasCrashLogs();
  if (ReceivedMemoryWarningBeforeLastShutdown()) {
    if (with_crash_log) {
      shutdown_type = SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_WITH_MEMORY_WARNING;
    } else {
      shutdown_type = SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_WITH_MEMORY_WARNING;
    }
  } else {
    if (with_crash_log) {
      shutdown_type = SHUTDOWN_IN_FOREGROUND_WITH_CRASH_LOG_NO_MEMORY_WARNING;
    } else {
      shutdown_type = SHUTDOWN_IN_FOREGROUND_NO_CRASH_LOG_NO_MEMORY_WARNING;
    }
  }
  LogShutdownType(shutdown_type);
}

bool IOSStabilityMetricsProvider::IsFirstLaunchAfterUpgrade() {
  return [[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade];
}

bool IOSStabilityMetricsProvider::HasCrashLogs() {
  return breakpad_helper::HasReportToUpload();
}

bool IOSStabilityMetricsProvider::HasUploadedCrashReportsInBackground() {
  return [CrashReportBackgroundUploader hasUploadedCrashReportsInBackground];
}

bool IOSStabilityMetricsProvider::ReceivedMemoryWarningBeforeLastShutdown() {
  return [[PreviousSessionInfo sharedInstance]
      didSeeMemoryWarningShortlyBeforeTerminating];
}
