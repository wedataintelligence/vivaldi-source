// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/test/pipeline_integration_test_base.h"

#include <utility>

#include "base/bind.h"
#include "base/features/features.h"
#include "base/memory/scoped_vector.h"
#include "media/base/cdm_context.h"
#include "media/base/media_log.h"
#include "media/base/test_data_util.h"
#include "media/filters/chunk_demuxer.h"
#if !defined(MEDIA_DISABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#endif
#include "media/filters/file_data_source.h"
#include "media/filters/opus_audio_decoder.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/renderer_impl.h"
#if !defined(MEDIA_DISABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#include <limits>
#include <queue>

#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/limits.h"
#if defined(OS_MACOSX)
#include "media/filters/at_audio_decoder.h"
#elif defined(OS_WIN)
#include "media/filters/wmf_audio_decoder.h"
#include "media/filters/wmf_video_decoder.h"
#endif
#include "media/filters/gpu_video_decoder.h"
#include "media/filters/ipc_demuxer.h"
#include "media/filters/pass_through_audio_decoder.h"
#include "media/filters/pass_through_video_decoder.h"
#include "media/renderers/mock_gpu_video_accelerator_factories.h"
#include "media/video/mock_video_decode_accelerator.h"
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace media {

const char kNullVideoHash[] = "d41d8cd98f00b204e9800998ecf8427e";
const char kNullAudioHash[] = "0.00,0.00,0.00,0.00,0.00,0.00,";

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
namespace {

const int kNumPictureBuffers = media::limits::kMaxVideoFrames + 1;
const int kMaxPictureWidth = 1920;
const int kMaxPictureHeight = 1080;

bool CreateTextures(int32_t count,
                    const gfx::Size& size,
                    std::vector<uint32_t>* texture_ids,
                    std::vector<gpu::Mailbox>* texture_mailboxes,
                    uint32_t texture_target) {
  CHECK_EQ(count, kNumPictureBuffers);
  for (int i = 0; i < count; ++i) {
    texture_ids->push_back(i + 1);
    texture_mailboxes->push_back(gpu::Mailbox());
  }
  return true;
}

VideoDecodeAccelerator::SupportedProfiles GetSupportedProfiles() {
  VideoDecodeAccelerator::SupportedProfile profile_prototype;
  profile_prototype.max_resolution.SetSize(std::numeric_limits<int>::max(),
                                           std::numeric_limits<int>::max());

  VideoDecodeAccelerator::SupportedProfiles all_profiles;
  for (int i = VIDEO_CODEC_PROFILE_MIN + 1; i <= VIDEO_CODEC_PROFILE_MAX; ++i) {
    profile_prototype.profile = static_cast<VideoCodecProfile>(i);
    all_profiles.push_back(profile_prototype);
  }

  return all_profiles;
}

}  // namespace

// A MockVideoDecodeAccelerator that pretends it reallly decodes.
class PipelineIntegrationTestBase::DecodingMockVDA
    : public MockVideoDecodeAccelerator {
 public:
  DecodingMockVDA() : client_(nullptr), enabled_(false) {
    EXPECT_CALL(*this, Initialize(_, _))
        .WillRepeatedly(testing::Invoke(this, &DecodingMockVDA::DoInitialize));
  }

  void Enable() {
    enabled_ = true;

    EXPECT_CALL(*this, AssignPictureBuffers(_))
        .WillRepeatedly(
            testing::Invoke(this, &DecodingMockVDA::SetPictureBuffers));
    EXPECT_CALL(*this, ReusePictureBuffer(_))
        .WillRepeatedly(
            testing::Invoke(this, &DecodingMockVDA::DoReusePictureBuffer));
    EXPECT_CALL(*this, Decode(_))
        .WillRepeatedly(testing::Invoke(this, &DecodingMockVDA::DoDecode));
    EXPECT_CALL(*this, Flush())
        .WillRepeatedly(testing::Invoke(this, &DecodingMockVDA::DoFlush));
  }

 private:
  enum { kFlush = -1 };

  bool DoInitialize(const Config &config, Client* client) {
    // This makes this VDA and GpuVideoDecoder unusable by default and will
    // require opt-in (see |Enable()|).
    if (!enabled_)
      return false;

    if (config.profile < media::H264PROFILE_MIN ||
      config.profile > media::H264PROFILE_MAX)
      return false;

    client_ = client;
    client_->ProvidePictureBuffers(
        kNumPictureBuffers, gfx::Size(kMaxPictureWidth, kMaxPictureHeight),
        GL_TEXTURE_RECTANGLE_ARB);
    return true;
  }

  void SetPictureBuffers(const std::vector<PictureBuffer>& buffers) {
    CHECK_EQ(buffers.size(), base::checked_cast<size_t>(kNumPictureBuffers));
    CHECK(available_picture_buffer_ids_.empty());

    for (const PictureBuffer& buffer : buffers)
      available_picture_buffer_ids_.push(buffer.id());
  }

  void DoReusePictureBuffer(int32_t id) {
    available_picture_buffer_ids_.push(id);
    if (!finished_bitstream_buffers_ids_.empty())
      SendPicture();
  }

  void DoDecode(const BitstreamBuffer& bitstream_buffer) {
    finished_bitstream_buffers_ids_.push(bitstream_buffer.id());

    if (!available_picture_buffer_ids_.empty())
      SendPicture();
  }

  void SendPicture() {
    CHECK(!available_picture_buffer_ids_.empty());
    CHECK(!finished_bitstream_buffers_ids_.empty());

    const int32_t bitstream_buffer_id = finished_bitstream_buffers_ids_.front();
    finished_bitstream_buffers_ids_.pop();

    client_->PictureReady(
        Picture(available_picture_buffer_ids_.front(), bitstream_buffer_id,
                gfx::Rect(kMaxPictureWidth, kMaxPictureHeight), false));
    available_picture_buffer_ids_.pop();

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&VideoDecodeAccelerator::Client::NotifyEndOfBitstreamBuffer,
                   base::Unretained(client_), bitstream_buffer_id));

    if (!finished_bitstream_buffers_ids_.empty() &&
        finished_bitstream_buffers_ids_.front() == kFlush) {
      finished_bitstream_buffers_ids_.pop();
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&VideoDecodeAccelerator::Client::NotifyFlushDone,
                     base::Unretained(client_)));
    }
  }

  void DoFlush() {
    // Enqueue a special "flush marker" picture.  It will be picked up in
    // |SendPicture()| when all the pictures enqueued before the marker have
    // been sent.
    finished_bitstream_buffers_ids_.push(kFlush);

    while (!finished_bitstream_buffers_ids_.empty() &&
           !available_picture_buffer_ids_.empty())
      SendPicture();
  }

  VideoDecodeAccelerator::Client* client_;
  std::queue<int32_t> available_picture_buffer_ids_;
  std::queue<int32_t> finished_bitstream_buffers_ids_;
  bool enabled_;
};
#endif

PipelineIntegrationTestBase::PipelineIntegrationTestBase()
    : hashing_enabled_(false),
      clockless_playback_(false),
      pipeline_(new Pipeline(message_loop_.task_runner(), new MediaLog())),
      ended_(false),
      pipeline_status_(PIPELINE_OK),
      last_video_frame_format_(PIXEL_FORMAT_UNKNOWN),
      last_video_frame_color_space_(COLOR_SPACE_UNSPECIFIED),
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
      mock_video_accelerator_factories_(
          new MockGpuVideoAcceleratorFactories(nullptr)),
      mock_vda_(new DecodingMockVDA),
      mse_mpeg_aac_enabler_(base::kFeatureMseAudioMpegAac, true),
#endif
      hardware_config_(AudioParameters(), AudioParameters()) {
  base::MD5Init(&md5_context_);
}

PipelineIntegrationTestBase::~PipelineIntegrationTestBase() {
  if (!pipeline_->IsRunning())
    return;

  Stop();
}

void PipelineIntegrationTestBase::OnSeeked(base::TimeDelta seek_time,
                                           PipelineStatus status) {
  EXPECT_EQ(seek_time, pipeline_->GetMediaTime());
  pipeline_status_ = status;
}

void PipelineIntegrationTestBase::OnStatusCallback(
    PipelineStatus status) {
  pipeline_status_ = status;
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

void PipelineIntegrationTestBase::DemuxerEncryptedMediaInitDataCB(
    EmeInitDataType type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(!init_data.empty());
  CHECK(!encrypted_media_init_data_cb_.is_null());
  encrypted_media_init_data_cb_.Run(type, init_data);
}

void PipelineIntegrationTestBase::OnEnded() {
  DCHECK(!ended_);
  ended_ = true;
  pipeline_status_ = PIPELINE_OK;
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

bool PipelineIntegrationTestBase::WaitUntilOnEnded() {
  if (ended_)
    return (pipeline_status_ == PIPELINE_OK);
  message_loop_.Run();
  EXPECT_TRUE(ended_);
  return ended_ && (pipeline_status_ == PIPELINE_OK);
}

PipelineStatus PipelineIntegrationTestBase::WaitUntilEndedOrError() {
  if (ended_ || pipeline_status_ != PIPELINE_OK)
    return pipeline_status_;
  message_loop_.Run();
  return pipeline_status_;
}

void PipelineIntegrationTestBase::OnError(PipelineStatus status) {
  DCHECK_NE(status, PIPELINE_OK);
  pipeline_status_ = status;
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

PipelineStatus PipelineIntegrationTestBase::Start(const std::string& filename) {
  return Start(filename, nullptr);
}

PipelineStatus PipelineIntegrationTestBase::Start(const std::string& filename,
                                                  CdmContext* cdm_context) {
  filename_ = filename;
  EXPECT_CALL(*this, OnMetadata(_))
      .Times(AtMost(1))
      .WillRepeatedly(SaveArg<0>(&metadata_));
  EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH))
      .Times(AnyNumber());
  EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_NOTHING))
      .Times(AnyNumber());
  CreateDemuxer(filename);

  if (cdm_context) {
    EXPECT_CALL(*this, DecryptorAttached(true));
    pipeline_->SetCdm(
        cdm_context, base::Bind(&PipelineIntegrationTestBase::DecryptorAttached,
                                base::Unretained(this)));
  }

  // Should never be called as the required decryption keys for the encrypted
  // media files are provided in advance.
  EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);

  pipeline_->Start(
      demuxer_.get(), CreateRenderer(GetTestDataFilePath(filename)),
      base::Bind(&PipelineIntegrationTestBase::OnEnded, base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnError, base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                 base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnMetadata,
                 base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnBufferingStateChanged,
                 base::Unretained(this)),
      base::Closure(), base::Bind(&PipelineIntegrationTestBase::OnAddTextTrack,
                                  base::Unretained(this)),
      base::Bind(&PipelineIntegrationTestBase::OnWaitingForDecryptionKey,
                 base::Unretained(this)));
  message_loop_.Run();
  return pipeline_status_;
}

PipelineStatus PipelineIntegrationTestBase::Start(const std::string& filename,
                                                  uint8_t test_type) {
  hashing_enabled_ = test_type & kHashed;
  clockless_playback_ = test_type & kClockless;
  return Start(filename);
}

void PipelineIntegrationTestBase::Play() {
  pipeline_->SetPlaybackRate(1);
}

void PipelineIntegrationTestBase::Pause() {
  pipeline_->SetPlaybackRate(0);
}

bool PipelineIntegrationTestBase::Seek(base::TimeDelta seek_time) {
  ended_ = false;

  EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH))
      .WillOnce(InvokeWithoutArgs(&message_loop_, &base::MessageLoop::QuitNow));
  pipeline_->Seek(seek_time, base::Bind(&PipelineIntegrationTestBase::OnSeeked,
                                        base::Unretained(this), seek_time));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

bool PipelineIntegrationTestBase::Suspend() {
  pipeline_->Suspend(base::Bind(&PipelineIntegrationTestBase::OnStatusCallback,
                                base::Unretained(this)));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

bool PipelineIntegrationTestBase::Resume(base::TimeDelta seek_time) {
  ended_ = false;

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  if (!mock_vda_)
    mock_vda_.reset(new DecodingMockVDA);
#endif

  EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH))
      .WillOnce(InvokeWithoutArgs(&message_loop_, &base::MessageLoop::QuitNow));
  pipeline_->Resume(CreateRenderer(GetTestDataFilePath(filename_)), seek_time,
                    base::Bind(&PipelineIntegrationTestBase::OnSeeked,
                               base::Unretained(this), seek_time));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::Stop() {
  DCHECK(pipeline_->IsRunning());
  pipeline_->Stop(base::MessageLoop::QuitWhenIdleClosure());
  message_loop_.Run();
}

void PipelineIntegrationTestBase::QuitAfterCurrentTimeTask(
    const base::TimeDelta& quit_time) {
  if (pipeline_->GetMediaTime() >= quit_time ||
      pipeline_status_ != PIPELINE_OK) {
    message_loop_.QuitWhenIdle();
    return;
  }

  message_loop_.PostDelayedTask(
      FROM_HERE,
      base::Bind(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                 base::Unretained(this), quit_time),
      base::TimeDelta::FromMilliseconds(10));
}

bool PipelineIntegrationTestBase::WaitUntilCurrentTimeIsAfter(
    const base::TimeDelta& wait_time) {
  DCHECK(pipeline_->IsRunning());
  DCHECK_GT(pipeline_->GetPlaybackRate(), 0);
  DCHECK(wait_time <= pipeline_->GetMediaDuration());

  message_loop_.PostDelayedTask(
      FROM_HERE,
      base::Bind(&PipelineIntegrationTestBase::QuitAfterCurrentTimeTask,
                 base::Unretained(this),
                 wait_time),
      base::TimeDelta::FromMilliseconds(10));
  message_loop_.Run();
  return (pipeline_status_ == PIPELINE_OK);
}

void PipelineIntegrationTestBase::CreateDemuxer(const std::string& filename) {
  FileDataSource* file_data_source = new FileDataSource();
  base::FilePath file_path(GetTestDataFilePath(filename));
  CHECK(file_data_source->Initialize(file_path)) << "Is " << file_path.value()
                                                 << " missing?";
  data_source_.reset(file_data_source);

  Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
      base::Bind(&PipelineIntegrationTestBase::DemuxerEncryptedMediaInitDataCB,
                 base::Unretained(this));

#if !defined(MEDIA_DISABLE_FFMPEG)
  demuxer_ = scoped_ptr<Demuxer>(
      new FFmpegDemuxer(message_loop_.task_runner(), data_source_.get(),
                        encrypted_media_init_data_cb, new MediaLog()));
#endif
}

scoped_ptr<Renderer> PipelineIntegrationTestBase::CreateRenderer(
    const base::FilePath& file_path) {
  ScopedVector<VideoDecoder> video_decoders;
  ScopedVector<AudioDecoder> audio_decoders;
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  const std::string content_type;
  const GURL url("file://" + file_path.AsUTF8Unsafe());
  if (IPCDemuxer::CanPlayType(content_type, url)) {
    audio_decoders.push_back(
        new PassThroughAudioDecoder(message_loop_.task_runner()));
    video_decoders.push_back(
        new PassThroughVideoDecoder(message_loop_.task_runner()));
  }

#if defined(OS_MACOSX)
  audio_decoders.push_back(
      new ATAudioDecoder(message_loop_.task_runner()));
#elif defined(OS_WIN)
  audio_decoders.push_back(
      new media::WMFAudioDecoder(message_loop_.task_runner()));
  video_decoders.push_back(
      new media::WMFVideoDecoder(message_loop_.task_runner()));
#endif

  video_decoders.push_back(
      new GpuVideoDecoder(mock_video_accelerator_factories_.get()));

  media::VideoDecodeAccelerator::Capabilities capabilities;
  capabilities.supported_profiles = GetSupportedProfiles();

  EXPECT_CALL(*mock_video_accelerator_factories_, GetTaskRunner())
      .WillRepeatedly(testing::Return(message_loop_.task_runner()));
  EXPECT_CALL(*mock_video_accelerator_factories_,
              GetVideoDecodeAcceleratorCapabilities())
      .WillRepeatedly(testing::Return(capabilities));
  EXPECT_CALL(*mock_video_accelerator_factories_,
              DoCreateVideoDecodeAccelerator())
      .WillRepeatedly(testing::Return(mock_vda_.get()));
  EXPECT_CALL(*mock_video_accelerator_factories_, CreateTextures(_, _, _, _, _))
      .WillRepeatedly(testing::Invoke(&CreateTextures));
  EXPECT_CALL(*mock_video_accelerator_factories_, DeleteTexture(_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_video_accelerator_factories_, WaitSyncToken(_))
      .Times(testing::AnyNumber());
  DCHECK(mock_vda_);
  EXPECT_CALL(*mock_vda_, Destroy())
      .WillRepeatedly(
          testing::Invoke(this, &PipelineIntegrationTestBase::DestroyMockVDA));
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

#if !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(new VpxVideoDecoder());
#endif  // !defined(MEDIA_DISABLE_LIBVPX)

#if !defined(MEDIA_DISABLE_FFMPEG)
  video_decoders.push_back(new FFmpegVideoDecoder());
#endif

  // Simulate a 60Hz rendering sink.
  video_sink_.reset(new NullVideoSink(
      clockless_playback_, base::TimeDelta::FromSecondsD(1.0 / 60),
      base::Bind(&PipelineIntegrationTestBase::OnVideoFramePaint,
                 base::Unretained(this)),
      message_loop_.task_runner()));

  // Disable frame dropping if hashing is enabled.
  scoped_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      message_loop_.task_runner(), message_loop_.task_runner().get(),
      video_sink_.get(), std::move(video_decoders), false, nullptr,
      new MediaLog()));

  if (!clockless_playback_) {
    audio_sink_ = new NullAudioSink(message_loop_.task_runner());
  } else {
    clockless_audio_sink_ = new ClocklessAudioSink();
  }

#if !defined(MEDIA_DISABLE_FFMPEG)
  audio_decoders.push_back(
      new FFmpegAudioDecoder(message_loop_.task_runner(), new MediaLog()));
#endif

  audio_decoders.push_back(
      new OpusAudioDecoder(message_loop_.task_runner()));

  // Don't allow the audio renderer to resample buffers if hashing is enabled.
  if (!hashing_enabled_) {
    AudioParameters out_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               CHANNEL_LAYOUT_STEREO,
                               44100,
                               16,
                               512);
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
    // Allow tests to specify their own output config.
    if (!hardware_config_.GetOutputConfig().IsValid())
#endif
    hardware_config_.UpdateOutputConfig(out_params);
  }

  scoped_ptr<AudioRenderer> audio_renderer(new AudioRendererImpl(
      message_loop_.task_runner(),
      (clockless_playback_)
          ? static_cast<AudioRendererSink*>(clockless_audio_sink_.get())
          : audio_sink_.get(),
      std::move(audio_decoders), hardware_config_, new MediaLog()));
  if (hashing_enabled_) {
    if (clockless_playback_)
      clockless_audio_sink_->StartAudioHashForTesting();
    else
      audio_sink_->StartAudioHashForTesting();
  }

  scoped_ptr<RendererImpl> renderer_impl(
      new RendererImpl(message_loop_.task_runner(), std::move(audio_renderer),
                       std::move(video_renderer)));

  // Prevent non-deterministic buffering state callbacks from firing (e.g., slow
  // machine, valgrind).
  renderer_impl->DisableUnderflowForTesting();

  if (clockless_playback_)
    renderer_impl->EnableClocklessVideoPlaybackForTesting();

  return std::move(renderer_impl);
}

void PipelineIntegrationTestBase::OnVideoFramePaint(
    const scoped_refptr<VideoFrame>& frame) {
  last_video_frame_format_ = frame->format();
  int result;
  if (frame->metadata()->GetInteger(VideoFrameMetadata::COLOR_SPACE, &result))
    last_video_frame_color_space_ = static_cast<ColorSpace>(result);
  if (!hashing_enabled_)
    return;
  VideoFrame::HashFrameForTesting(&md5_context_, frame);
}

std::string PipelineIntegrationTestBase::GetVideoHash() {
  DCHECK(hashing_enabled_);
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context_);
  return base::MD5DigestToBase16(digest);
}

std::string PipelineIntegrationTestBase::GetAudioHash() {
  DCHECK(hashing_enabled_);

  if (clockless_playback_)
    return clockless_audio_sink_->GetAudioHashForTesting();
  return audio_sink_->GetAudioHashForTesting();
}

base::TimeDelta PipelineIntegrationTestBase::GetAudioTime() {
  DCHECK(clockless_playback_);
  return clockless_audio_sink_->render_time();
}

base::TimeTicks DummyTickClock::NowTicks() {
  now_ += base::TimeDelta::FromSeconds(60);
  return now_;
}

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
void PipelineIntegrationTestBase::EnableMockVDA() {
  mock_vda_->Enable();
}

void PipelineIntegrationTestBase::DestroyMockVDA() {
  mock_vda_.reset();
}
#endif

}  // namespace media