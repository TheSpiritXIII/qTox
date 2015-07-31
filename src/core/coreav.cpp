/*
    Copyright (C) 2013 by Maxim Biro <nurupo.contributions@gmail.com>
    Copyright © 2014-2015 by The qTox Project

    This file is part of qTox, a Qt-based graphical interface for Tox.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    qTox is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with qTox.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "core.h"
#include "src/video/camerasource.h"
#include "src/video/corevideosource.h"
#include "src/video/videoframe.h"
#include "src/audio/audio.h"
#ifdef QTOX_FILTER_AUDIO
#include "src/audio/audiofilterer.h"
#endif
#include "src/persistence/settings.h"
#include <assert.h>
#include <QDebug>
#include <QTimer>

//OLD:ToxCall Core::calls[TOXAV_MAX_CALLS];
QHash<uint32_t, ToxCall> Core::halls;
#ifdef QTOX_FILTER_AUDIO
//OLD:AudioFilterer * Core::filterer[TOXAV_MAX_CALLS] {nullptr};
QHash<uint32_t, AudioFilterer*> Core::hilterer;
#endif
const int Core::videobufsize{TOXAV_MAX_VIDEO_WIDTH * TOXAV_MAX_VIDEO_HEIGHT * 4};
uint8_t* Core::videobuf;

bool Core::anyActiveCalls()
{
    for (auto& call : halls)
    {
        if (call.active)
            return true;
    }
    return false;
}

void Core::prepareCall(uint32_t friendId, ToxAV* toxav, bool videoEnabled)
{
    qDebug() << QString("preparing call %1").arg(friendId);

    if (!videobuf)
        videobuf = new uint8_t[videobufsize];

    halls[friendId].muteVol = false;
    halls[friendId].muteMic = false;
    halls[friendId].videoEnabled = videoEnabled;

    // Audio
    Audio::suscribeInput();

    // Go
    halls[friendId].active = true;
    halls[friendId].alSource = 0;
    halls[friendId].sendAudioTimer = new QTimer();
    halls[friendId].sendAudioTimer->moveToThread(coreThread);
    halls[friendId].sendAudioTimer->setInterval(5);
    halls[friendId].sendAudioTimer->setSingleShot(true);
    connect(halls[friendId].sendAudioTimer, &QTimer::timeout, [=](){sendCallAudio(friendId,toxav);});
    halls[friendId].sendAudioTimer->start();
    if (halls[friendId].videoEnabled)
    {
        // OLD:
        /*
        calls[callId].videoSource = new CoreVideoSource;
        CameraSource& source = CameraSource::getInstance();
        source.subscribe();
        connect(&source, &VideoSource::frameAvailable,
                [=](std::shared_ptr<VideoFrame> frame){sendCallVideo(callId,toxav,frame);});*/
    }

#ifdef QTOX_FILTER_AUDIO
    if (Settings::getInstance().getFilterAudio())
    {
        hilterer.insert(friendId, new AudioFilterer()).value()->startFilter(48000);
    }
    else
    {
        delete hilterer[friendId];
        hilterer.remove(friendId);
    }
#endif
    //OLD:
    /*
    calls[callId].callId = callId;
    calls[callId].friendId = friendId;
    // the following three lines are also now redundant from startCall, but are
    // necessary there for outbound and here for inbound
    calls[callId].codecSettings = av_DefaultSettings;
    calls[callId].codecSettings.max_video_width = TOXAV_MAX_VIDEO_WIDTH;
    calls[callId].codecSettings.max_video_height = TOXAV_MAX_VIDEO_HEIGHT;
    int r = toxav_prepare_transmission(toxav, callId, videoEnabled);
    if (r < 0)
        qWarning() << QString("Error starting call %1: toxav_prepare_transmission failed with %2").arg(callId).arg(r);*/

}

void Core::onAvMediaChange(void* toxav, int32_t callId, void* core)
{
    //OLD:
    /*
    int friendId;
    int cap = toxav_capability_supported((ToxAv*)toxav, callId,
                        (ToxAvCapabilities)(av_VideoEncoding|av_VideoDecoding));
    if (!cap)
        goto fail;

    friendId  = toxav_get_peer_id((ToxAv*)toxav, callId, 0);
    if (friendId < 0)
        goto fail;

    qDebug() << "Received media change from friend "<<friendId;

    if (cap == (av_VideoEncoding|av_VideoDecoding)) // Video call
    {
        emit static_cast<Core*>(core)->avMediaChange(friendId, callId, true);
        calls[callId].videoSource = new CoreVideoSource;
        CameraSource& source = CameraSource::getInstance();
        source.subscribe();
        calls[callId].videoEnabled = true;
    }
    else // Audio call
    {
        emit static_cast<Core*>(core)->avMediaChange(friendId, callId, false);
        calls[callId].videoEnabled = false;
        CameraSource::getInstance().unsubscribe();
        calls[callId].videoSource->setDeleteOnClose(true);
        calls[callId].videoSource = nullptr;
    }

    return;

fail: // Centralized error handling
    qWarning() << "Toxcore error while receiving media change on call "<<callId;
    return;*/
}

void Core::answerCall(uint32_t friendId)
{
    qDebug() << QString("answering call %1").arg(friendId);

    //NOTE: Change callId to friendId.
    TOXAV_ERR_ANSWER answer_err;

    if (toxav_answer(newtox_av, friendId, 48, 0, &answer_err))
    {
        halls[friendId].active = true;
    }
    else
    {
        qWarning() << "Answer failed";
        return;
    }

    switch (answer_err)
    {
        case TOXAV_ERR_ANSWER_OK:
            break;
        case TOXAV_ERR_ANSWER_CODEC_INITIALIZATION:
            qWarning() << "Codec initialization error";
            break;
        case TOXAV_ERR_ANSWER_FRIEND_NOT_FOUND:
            qWarning() << "Friend not found";
            break;
        case TOXAV_ERR_ANSWER_FRIEND_NOT_CALLING:
            qWarning() << "Friend not calling";
            break;
        case TOXAV_ERR_ANSWER_INVALID_BIT_RATE:
            qWarning() << "Invalid bit rate";
            break;
    }

    //OLD:
    /*int friendId = toxav_get_peer_id(toxav, callId, 0);
    if (friendId < 0)
    {
        qWarning() << "Received invalid AV answer peer ID";
        return;
    }

    ToxAvCSettings* transSettings = new ToxAvCSettings;
    int err = toxav_get_peer_csettings(toxav, callId, 0, transSettings);
    if (err != av_ErrorNone)
    {
         qWarning() << "answerCall: error getting call settings";
         delete transSettings;
         return;
    }

    if (transSettings->call_type == av_TypeVideo)
    {
        qDebug() << QString("answering call %1 with video").arg(callId);
        toxav_answer(toxav, callId, transSettings);
    }
    else
    {
        qDebug() << QString("answering call %1 without video").arg(callId);
        toxav_answer(toxav, callId, transSettings);
    }

    delete transSettings;*/
}

void Core::hangupCall(int32_t callId)
{
    qDebug() << QString("hanging up call %1").arg(callId);
    //OLD:calls[callId].active = false;
    //OLD:toxav_hangup(toxav, callId);
}

void Core::rejectCall(int32_t callId)
{
    // NOTE: Change callId to friendId.
    qDebug() << QString("rejecting call %1").arg(callId);

    TOXAV_ERR_CALL_CONTROL control_err;
    toxav_call_control(newtox_av, callId, TOXAV_CALL_CONTROL_CANCEL, &control_err);

    handleCallError(control_err);
}

void Core::startCall(uint32_t friendId, bool video)
{
    if (video)
        qDebug() << QString("Starting new call with %1 with video").arg(friendId);
    else
        qDebug() << QString("Starting new call with %1").arg(friendId);

    TOXAV_ERR_CALL call_error;

    if (toxav_call(newtox_av, friendId, 48, 0, &call_error))
    {
        emit avRinging(friendId, 0, video);
    }
    else
    {
        qWarning() << QString("Failed to start new video call with %1").arg(friendId);
        emit avCallFailed(friendId);
        return;
    }

    switch (call_error)
    {
        case TOXAV_ERR_CALL_OK:
            break;
        case TOXAV_ERR_CALL_MALLOC:
            qWarning() << "Allocation error";
            break;
        case TOXAV_ERR_CALL_FRIEND_NOT_FOUND:
            qWarning() << "Friend not found";
            break;
        case TOXAV_ERR_CALL_FRIEND_NOT_CONNECTED:
            qWarning() << "Friend not connected";
            break;
        case TOXAV_ERR_CALL_FRIEND_ALREADY_IN_CALL:
            qWarning() << "Friend already in call";
            break;
        case TOXAV_ERR_CALL_INVALID_BIT_RATE:
            qWarning() << "Invalid bit rate";
            break;
    }

    //OLD:
    /*int32_t callId;
    ToxAvCSettings cSettings = av_DefaultSettings;
    cSettings.max_video_width = TOXAV_MAX_VIDEO_WIDTH;
    cSettings.max_video_height = TOXAV_MAX_VIDEO_HEIGHT;
    if (video)
    {
        qDebug() << QString("Starting new call with %1 with video").arg(friendId);
        cSettings.call_type = av_TypeVideo;
        if (toxav_call(toxav, &callId, friendId, &cSettings, TOXAV_RINGING_TIME) == 0)
        {
            calls[callId].videoEnabled=true;
        }
        else
        {
            qWarning() << QString("Failed to start new video call with %1").arg(friendId);
            emit avCallFailed(friendId);
            return;
        }
    }
    else
    {
        qDebug() << QString("Starting new call with %1 without video").arg(friendId);
        cSettings.call_type = av_TypeAudio;
        if (toxav_call(toxav, &callId, friendId, &cSettings, TOXAV_RINGING_TIME) == 0)
        {
            calls[callId].videoEnabled=false;
        }
        else
        {
            qWarning() << QString("Failed to start new audio call with %1").arg(friendId);
            emit avCallFailed(friendId);
            return;
        }
    }*/
}

void Core::cancelCall(uint32_t friendId)
{
    qDebug() << QString("Cancelling call with %1").arg(friendId);
    //OLD:calls[callId].active = false;

    TOXAV_ERR_CALL_CONTROL control_err;
    toxav_call_control(newtox_av, friendId, TOXAV_CALL_CONTROL_CANCEL, &control_err);

    handleCallError(control_err);
}

void Core::cleanupCall(int32_t callId)
{
    //OLD:
    /*assert(calls[callId].active);
    qDebug() << QString("cleaning up call %1").arg(callId);
    calls[callId].active = false;
    disconnect(calls[callId].sendAudioTimer,0,0,0);
    calls[callId].sendAudioTimer->stop();
    if (calls[callId].videoEnabled)
    {
        CameraSource::getInstance().unsubscribe();
        if (calls[callId].videoSource)
        {
            calls[callId].videoSource->setDeleteOnClose(true);
            calls[callId].videoSource = nullptr;
        }
    }

    Audio::unsuscribeInput();
    //OLD:toxav_kill_transmission(Core::getInstance()->toxav, callId);

    if (!anyActiveCalls())
    {
        delete[] videobuf;
        videobuf = nullptr;
    }*/
}

void Core::playCallAudio(ToxAV* toxav, uint32_t friendId, const int16_t *data, size_t samples, uint8_t channels, uint32_t sampling_rate, void* user_data)
{
    Q_UNUSED(user_data);

    if (!halls[friendId].active || halls[friendId].muteVol)
        return;

    if (!halls[friendId].alSource)
        alGenSources(1, &halls[friendId].alSource);

    playAudioBuffer(halls[friendId].alSource, data, samples, channels, sampling_rate);
}

void Core::sendCallAudio(uint32_t friendId, ToxAV* toxav)
{
    if (!halls[friendId].active)
        return;

    if (halls[friendId].muteMic || !Audio::isInputReady())
    {
        halls[friendId].sendAudioTimer->start();
        return;
    }

    uint8_t buf[Audio::bufsize];

    if (Audio::tryCaptureSamples(buf, Audio::framesize, Audio::bufsize))
    {
#ifdef QTOX_FILTER_AUDIO
        if (Settings::getInstance().getFilterAudio())
        {
            if (!hilterer[friendId])
            {
                hilterer[friendId] = new AudioFilterer();
                hilterer[friendId]->startFilter(48000);
            }
            // is a null op #ifndef ALC_LOOPBACK_CAPTURE_SAMPLES
            Audio::getEchoesToFilter(hilterer[friendId], Audio::framesize);

            hilterer[friendId]->filterAudio((int16_t*) buf, Audio::framesize);
        }
        else if (hilterer.find(friendId) != hilterer.end())
        {
            delete hilterer[friendId];
            hilterer.remove(friendId);
        }
#endif
        TOXAV_ERR_SEND_FRAME frame_err;

        if (!toxav_audio_send_frame(toxav, friendId, (int16_t*)buf, Audio::sample_count, Audio::channels, Audio::sample_rate, &frame_err))
        {
            qDebug() << "toxav_audio_send_frame error";

            switch (frame_err)
            {
                case TOXAV_ERR_SEND_FRAME_OK:
                    break;
                case TOXAV_ERR_SEND_FRAME_NULL:
                    qWarning() << "Frame null";
                    break;
                case TOXAV_ERR_SEND_FRAME_FRIEND_NOT_FOUND:
                    qWarning() << "Friend not found";
                    break;
                case TOXAV_ERR_SEND_FRAME_FRIEND_NOT_IN_CALL:
                    qWarning() << "Friend not in call";
                    break;
                case TOXAV_ERR_SEND_FRAME_INVALID:
                    qWarning() << "Frame invalid";
                    break;
                case TOXAV_ERR_SEND_FRAME_PAYLOAD_TYPE_DISABLED:
                    qWarning() << "Payload type disabled";
                    break;
                case TOXAV_ERR_SEND_FRAME_RTP_FAILED:
                    qWarning() << "Frame RTP failed";
                    break;
            }
        }
        else
        {
            qDebug() << "Hapy camper";
        }
        // OLD:
        /*
        int r;
        if ((r = toxav_prepare_audio_frame(toxav, callId, dest, framesize*2, (int16_t*)buf, framesize)) < 0)
        {
            qDebug() << "toxav_prepare_audio_frame error";
            calls[callId].sendAudioTimer->start();
            return;
        }

        if ((r = toxav_send_audio(toxav, callId, dest, r)) < 0)
            */
    }
    halls[friendId].sendAudioTimer->start();
}

//OLD:
/*void Core::playCallVideo(void*, int32_t callId, const vpx_image_t* img, void *user_data)
{switch (control_err)
    {
        case TOXAV_ERR_CALL_CONTROL_OK:
            break;
        case TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_FOUND:
            qWarning() << "Friend not found";
            break;
        case TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_IN_CALL:
            qWarning() << "Friend not in a call";
            break;
        case TOXAV_ERR_CALL_CONTROL_INVALID_TRANSITION:
            qWarning() << "Invalid transition";
            break;
    }
    Q_UNUSED(user_data);

    if (!calls[callId].active || !calls[callId].videoEnabled)
        return;

    calls[callId].videoSource->pushFrame(img);
}*/

//OLD:
/*void Core::sendCallVideo(int32_t callId, ToxAv* toxav, std::shared_ptr<VideoFrame> vframe)
{
    if (!calls[callId].active || !calls[callId].videoEnabled)
        return;

    // This frame shares vframe's buffers, we don't call vpx_img_free but just delete it
    vpx_image* frame = vframe->toVpxImage();
    if (frame->fmt == VPX_IMG_FMT_NONE)
    {
        qWarning() << "Invalid frame";
        delete frame;
        return;
    }

    int result;
    if ((result = toxav_prepare_video_frame(toxav, callId, videobuf, videobufsize, frame)) < 0)
    {
        qDebug() << QString("toxav_prepare_video_frame: error %1").arg(result);
        delete frame;
        return;
    }

    if ((result = toxav_send_video(toxav, callId, (uint8_t*)videobuf, result)) < 0)
        qDebug() << QString("toxav_send_video error: %1").arg(result);

    delete frame;
}*/

void Core::micMuteToggle(int32_t callId)
{
    //OLD:
    /*
    if (calls[callId].active)
        calls[callId].muteMic = !calls[callId].muteMic;*/
}

void Core::volMuteToggle(int32_t callId)
{
    //OLD:
    /*if (calls[callId].active)
        calls[callId].muteVol = !calls[callId].muteVol;*/
}

void Core::onAvCancel(ToxAV* toxav, uint32_t friendId, void* core)
{
    //OLD:
    /*
    ToxAv* toxav = static_cast<ToxAv*>(_toxav);

    int friendId = toxav_get_peer_id(toxav, callId, 0);
    if (friendId < 0)
    {
        qWarning() << "Received invalid AV cancel";
        return;
    }
    qDebug() << QString("AV cancel from %1").arg(friendId);

    calls[callId].active = false;

#ifdef QTOX_FILTER_AUDIO
    if (filterer[callId])
    {
        filterer[callId]->closeFilter();
        delete filterer[callId];
        filterer[callId] = nullptr;
    }
#endif*/

    if (halls[friendId].active)
    {
        qDebug() << QString("AV cancel from %1").arg(friendId);
        emit static_cast<Core*>(core)->avCancel(friendId);
    }
    else
    {
        qDebug() << QString("AV reject from %1").arg(friendId);
        emit static_cast<Core*>(core)->avRejected(friendId);
    }
}

void Core::onAvReject(void* _toxav, int32_t callId, void* core)
{
    //OLD:
    /*ToxAv* toxav = static_cast<ToxAv*>(_toxav);
    int friendId = toxav_get_peer_id(toxav, callId, 0);
    if (friendId < 0)
    {
        qWarning() << "Received invalid AV reject";
        return;
    }

    qDebug() << QString("AV reject from %1").arg(friendId);

    emit static_cast<Core*>(core)->avRejected(friendId, callId);*/
}

void Core::onAvEnd(void* _toxav, int32_t call_index, void* core)
{
    //OLD:
    /*ToxAv* toxav = static_cast<ToxAv*>(_toxav);

    int friendId = toxav_get_peer_id(toxav, call_index, 0);
    if (friendId < 0)
    {
        qWarning() << "Received invalid AV end";
        return;
    }
    qDebug() << QString("AV end from %1").arg(friendId);

    emit static_cast<Core*>(core)->avEnd(friendId, call_index);

    if (calls[call_index].active)
        cleanupCall(call_index);*/
}

void Core::onAvRinging(void* _toxav, int32_t call_index, void* core)
{
    //OLD:
    /*ToxAv* toxav = static_cast<ToxAv*>(_toxav);

    int friendId = toxav_get_peer_id(toxav, call_index, 0);
    if (friendId < 0)
    {
        qWarning() << "Received invalid AV ringing";
        return;
    }

    if (calls[call_index].videoEnabled)
    {
        qDebug() << QString("AV ringing with %1 with video").arg(friendId);
        emit static_cast<Core*>(core)->avRinging(friendId, call_index, true);
    }
    else
    {
        qDebug() << QString("AV ringing with %1 without video").arg(friendId);
        emit static_cast<Core*>(core)->avRinging(friendId, call_index, false);
    }*/
}

void Core::onAvRequestTimeout(void* _toxav, int32_t call_index, void* core)
{
    //OLD:
    /*ToxAv* toxav = static_cast<ToxAv*>(_toxav);

    int friendId = toxav_get_peer_id(toxav, call_index, 0);
    if (friendId < 0)
    {
        qWarning() << "Received invalid AV request timeout";
        return;
    }
    qDebug() << QString("AV request timeout with %1").arg(friendId);

    emit static_cast<Core*>(core)->avRequestTimeout(friendId, call_index);

    if (calls[call_index].active)
        cleanupCall(call_index);*/
}

void Core::onAvPeerTimeout(void* _toxav, int32_t call_index, void* core)
{
    //OLD:
    /*ToxAv* toxav = static_cast<ToxAv*>(_toxav);

    int friendId = toxav_get_peer_id(toxav, call_index, 0);
    if (friendId < 0)
    {
        qWarning() << "Received invalid AV peer timeout";
        return;
    }
    qDebug() << QString("AV peer timeout with %1").arg(friendId);

    emit static_cast<Core*>(core)->avPeerTimeout(friendId, call_index);

    if (calls[call_index].active)
        cleanupCall(call_index);*/
}


void Core::onAvInvite(ToxAV* _toxav, uint32_t friendId, bool audio_enable, bool video_enabled, void* core)
{
    qDebug() << "Invited by " << friendId << audio_enable << video_enabled;

    emit static_cast<Core*>(core)->avInvite(friendId, 0, video_enabled);
    //OLD:
    /*ToxAv* toxav = static_cast<ToxAv*>(_toxav);

    int friendId = toxav_get_peer_id(toxav, call_index, 0);
    if (friendId < 0)
    {
        qWarning() << "Received invalid AV invite";
        return;
    }

    ToxAvCSettings* transSettings = new ToxAvCSettings;
    int err = toxav_get_peer_csettings(toxav, call_index, 0, transSettings);
    if (err != av_ErrorNone)
    {
        qWarning() << "onAvInvite: error getting call type";
        delete transSettings;
        return;
    }

    if (transSettings->call_type == av_TypeVideo)
    {
        qDebug() << QString("AV invite from %1 with video").arg(friendId);
        emit static_cast<Core*>(core)->avInvite(friendId, call_index, true);
    }
    else
    {
        qDebug() << QString("AV invite from %1 without video").arg(friendId);
        emit static_cast<Core*>(core)->avInvite(friendId, call_index, false);
    }

    delete transSettings;*/
}

void Core::onAvState(ToxAV *toxav, uint32_t friendId, uint32_t state, void *core)
{
    qDebug() << "AV state changed." << state;

    if (state & TOXAV_FRIEND_CALL_STATE_ERROR)
    {
        qWarning() << "Call state error.";
    }
    if (state & TOXAV_FRIEND_CALL_STATE_FINISHED)
    {
        onAvCancel(toxav, friendId, core);
    }
    if (state & TOXAV_FRIEND_CALL_STATE_SENDING_A)
    {
        onAvStart(toxav, friendId, core);
    }
    if (state & TOXAV_FRIEND_CALL_STATE_SENDING_V)
    {
        qDebug() << "Unimplemented sending video";
    }
    if (state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_A)
    {
        qDebug() << "Unimplemented sending video";
    }
    if (state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_V)
    {
        qDebug() << "Unimplemented sending video";
    }
}

void Core::onAvStart(ToxAV* toxav, uint32_t friendId, void* core)
{
    if (false)
    {
        qDebug() << QString("AV start from %1 with video").arg(friendId);
        prepareCall(friendId, toxav, true);
        emit static_cast<Core*>(core)->avStart(friendId, friendId, true);
    }
    else
    {
        qDebug() << QString("AV start from %1 without video").arg(friendId);
        prepareCall(friendId, toxav, false);
        emit static_cast<Core*>(core)->avStart(friendId, friendId, false);
    }

    //OLD:
    /*ToxAv* toxav = static_cast<ToxAv*>(_toxav);

    int friendId = toxav_get_peer_id(toxav, call_index, 0);
    if (friendId < 0)
    {
        qWarning() << "Received invalid AV start";
        return;
    }

    ToxAvCSettings* transSettings = new ToxAvCSettings;
    int err = toxav_get_peer_csettings(toxav, call_index, 0, transSettings);
    if (err != av_ErrorNone)
    {
        qWarning() << "onAvStart: error getting call type";
        delete transSettings;
        return;
    }

    if (transSettings->call_type == av_TypeVideo)
    {
        qDebug() << QString("AV start from %1 with video").arg(friendId);
        prepareCall(friendId, call_index, toxav, true);
        emit static_cast<Core*>(core)->avStart(friendId, call_index, true);
    }
    else
    {
        qDebug() << QString("AV start from %1 without video").arg(friendId);
        prepareCall(friendId, call_index, toxav, false);
        emit static_cast<Core*>(core)->avStart(friendId, call_index, false);
    }

    delete transSettings;*/
}

// This function's logic was shamelessly stolen from uTox
void Core::playAudioBuffer(ALuint alSource, const int16_t *data, int samples, unsigned channels, int sampleRate)
{
    if (!channels || channels > 2)
    {
        qWarning() << "playAudioBuffer: trying to play on "<<channels<<" channels! Giving up.";
        return;
    }

    ALuint bufid;
    ALint processed = 0, queued = 16;
    alGetSourcei(alSource, AL_BUFFERS_PROCESSED, &processed);
    alGetSourcei(alSource, AL_BUFFERS_QUEUED, &queued);
    alSourcei(alSource, AL_LOOPING, AL_FALSE);

    if (processed)
    {
        ALuint bufids[processed];
        alSourceUnqueueBuffers(alSource, processed, bufids);
        alDeleteBuffers(processed - 1, bufids + 1);
        bufid = bufids[0];
    }
    else if (queued < 32)
    {
        alGenBuffers(1, &bufid);
    }
    else
    {
        qDebug() << "Dropped audio frame";
        return;
    }

    alBufferData(bufid, (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, data,
                    samples * 2 * channels, sampleRate);
    alSourceQueueBuffers(alSource, 1, &bufid);

    ALint state;
    alGetSourcei(alSource, AL_SOURCE_STATE, &state);
    alSourcef(alSource, AL_GAIN, Audio::getOutputVolume());
    if (state != AL_PLAYING)
    {
        alSourcePlay(alSource);
        //qDebug() << "Starting audio source " << (int)alSource;
    }
}

void Core::handleCallError(TOXAV_ERR_CALL_CONTROL control_err)
{
    switch (control_err)
    {
        case TOXAV_ERR_CALL_CONTROL_OK:
            break;
        case TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_FOUND:
            qWarning() << "Friend not found";
            break;
        case TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_IN_CALL:
            qWarning() << "Friend not in a call";
            break;
        case TOXAV_ERR_CALL_CONTROL_INVALID_TRANSITION:
            qWarning() << "Invalid transition";
            break;
    }
}

VideoSource *Core::getVideoSourceFromCall(int callNumber)
{
    //OLD:return calls[callNumber].videoSource;
    return nullptr;
}

void Core::joinGroupCall(int groupId)
{
    //OLD:
    /*qDebug() << QString("Joining group call %1").arg(groupId);
    groupCalls[groupId].groupId = groupId;
    groupCalls[groupId].muteMic = false;
    groupCalls[groupId].muteVol = false;
    // the following three lines are also now redundant from startCall, but are
    // necessary there for outbound and here for inbound
    groupCalls[groupId].codecSettings = av_DefaultSettings;
    groupCalls[groupId].codecSettings.max_video_width = TOXAV_MAX_VIDEO_WIDTH;
    groupCalls[groupId].codecSettings.max_video_height = TOXAV_MAX_VIDEO_HEIGHT;

    // Audio
    Audio::suscribeInput();

    // Go
    Core* core = Core::getInstance();
    ToxAv* toxav = core->toxav;

    groupCalls[groupId].sendAudioTimer = new QTimer();
    groupCalls[groupId].active = true;
    groupCalls[groupId].sendAudioTimer->setInterval(5);
    groupCalls[groupId].sendAudioTimer->setSingleShot(true);
    connect(groupCalls[groupId].sendAudioTimer, &QTimer::timeout, [=](){sendGroupCallAudio(groupId,toxav);});
    groupCalls[groupId].sendAudioTimer->start();*/
}

void Core::leaveGroupCall(int groupId)
{
    qDebug() << QString("Leaving group call %1").arg(groupId);
    groupCalls[groupId].active = false;
    disconnect(groupCalls[groupId].sendAudioTimer,0,0,0);
    groupCalls[groupId].sendAudioTimer->stop();
    for (ALuint source : groupCalls[groupId].alSources)
        alDeleteSources(1, &source);
    groupCalls[groupId].alSources.clear();
    Audio::unsuscribeInput();
    delete groupCalls[groupId].sendAudioTimer;
}

//OLD:
/*void Core::sendGroupCallAudio(int groupId, ToxAv* toxav)
{
    if (!groupCalls[groupId].active)
        return;

    if (groupCalls[groupId].muteMic || !Audio::isInputReady())
    {
        groupCalls[groupId].sendAudioTimer->start();
        return;
    }

    const int framesize = (groupCalls[groupId].codecSettings.audio_frame_duration * groupCalls[groupId].codecSettings.audio_sample_rate) / 1000 * av_DefaultSettings.audio_channels;
    const int bufsize = framesize * 2 * av_DefaultSettings.audio_channels;
    uint8_t buf[bufsize];

    if (Audio::tryCaptureSamples(buf, framesize))
    {
        if (toxav_group_send_audio(toxav_get_tox(toxav), groupId, (int16_t*)buf,
                framesize, av_DefaultSettings.audio_channels, av_DefaultSettings.audio_sample_rate) < 0)
        {
            qDebug() << "toxav_group_send_audio error";
            groupCalls[groupId].sendAudioTimer->start();
            return;
        }
    }
    groupCalls[groupId].sendAudioTimer->start();
}*/

void Core::disableGroupCallMic(int groupId)
{
    groupCalls[groupId].muteMic = true;
}

void Core::disableGroupCallVol(int groupId)
{
    groupCalls[groupId].muteVol = true;
}

void Core::enableGroupCallMic(int groupId)
{
    groupCalls[groupId].muteMic = false;
}

void Core::enableGroupCallVol(int groupId)
{
    groupCalls[groupId].muteVol = false;
}

bool Core::isGroupCallMicEnabled(int groupId)
{
    return !groupCalls[groupId].muteMic;
}

bool Core::isGroupCallVolEnabled(int groupId)
{
    return !groupCalls[groupId].muteVol;
}
