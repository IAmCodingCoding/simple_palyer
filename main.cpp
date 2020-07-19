#define SDL_MAIN_HANDLED

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

#include "SDL2/SDL.h"
#include "SDL2/SDL_thread.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <list>
#include <stdio.h>
#include "AudioResampler.h"
#include "VideoScalar.h"
#include "libyuv.h"

std::list<AVFrame *> audio_frames;
std::mutex ma;


std::list<AVFrame *> video_frames;
std::mutex mv;

AVFormatContext *pFmtCtx = nullptr;
// 找到第一个视频流
int videoStream = -1;
int audioStream = -1;

AVCodecContext *videoCodecContext = nullptr;
AVCodecContext *audioCodecContext = nullptr;

SDL_Renderer *renderer = nullptr;

SDL_Texture *texture = nullptr;

AVFrame *frame = nullptr;
double video_time_base = 0.0;
double audio_time_base = 0.0;
int width = 0;
int height = 0;

int channels;

int sample_rate;

int nb_samples;
AudioResampler *resampler = nullptr;

VideoScalar *videoScalar = nullptr;


void fill_audio(void *userdata, Uint8 *stream,
                int len) {
    std::unique_lock<std::mutex> lock(ma);
    SDL_memset(stream, 0, len);
    if (!audio_frames.empty()) {
        AVFrame *f = audio_frames.front();
        memcpy(stream, f->data[0], len);
        long pts = (long) ((f->pts * audio_time_base) * 1000);
        audio_frames.pop_front();
        av_freep(&f->data[0]);
        av_frame_free(&f);
        //sync  同步播放视频视频帧
        std::unique_lock<std::mutex> lock(mv);
        while (!video_frames.empty()) {
            AVFrame *vf = video_frames.front();
            long v_pts = (long) ((vf->pts * video_time_base) * 1000);
            if (v_pts < pts) {
                if (pts - v_pts > 45) {
                    video_frames.pop_front();
                    av_freep(&vf->data[0]);
                    av_frame_free(&vf);
                } else {
                    SDL_Rect rc = {0, 0, width, height};
//                    SDL_UpdateYUVTexture(texture, &rc, vf->data[0], vf->linesize[0], vf->data[1],
//                                         vf->linesize[1], vf->data[2], vf->linesize[2]);
                    SDL_UpdateTexture(texture, &rc, vf->data[0], vf->linesize[0]);
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, &rc, &rc);
                    SDL_RenderPresent(renderer);
                    video_frames.pop_front();
                    av_freep(&vf->data[0]);
                    av_frame_free(&vf);
                }
            } else {
                if (v_pts - pts > 45) {
                    break;
                } else {
                    SDL_Rect rc = {0, 0, width, height};
//                    SDL_UpdateYUVTexture(texture, &rc, vf->data[0], vf->linesize[0], vf->data[1],
//                                         vf->linesize[1], vf->data[2], vf->linesize[2]);
                    SDL_UpdateTexture(texture, &rc, vf->data[0], vf->linesize[0]);//upload rgb data to gpu
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, &rc, &rc);
                    SDL_RenderPresent(renderer);
                    video_frames.pop_front();
                    av_freep(&vf->data[0]);
                    av_frame_free(&vf);
                }
            }
        }
    } else {//暂时没有音频
        uint8_t *data[4];
        int linesize[4];
        av_samples_alloc(data, linesize, channels, nb_samples ? nb_samples : 1024, AV_SAMPLE_FMT_FLTP, 1);
        av_samples_set_silence(data, 0, 0, channels, AV_SAMPLE_FMT_FLTP);
        memcpy(stream, data[0], len < linesize[0] ? len : linesize[0]);
        av_freep(data[0]);
    }
}

int main() {
    // 初始化 SDL 库
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        std::cerr << "Could not initialize SDL - " << SDL_GetError() << std::endl;
        return -1;
    }

    pFmtCtx = NULL;
    // 打开视频文件，读取文件头信息到 AVFormatContext 结构体中
    if (int ret = avformat_open_input(&pFmtCtx, "../test_data/input.mp4", NULL, NULL) != 0) {
        return -1;
    }

    // 读取流信息到 AVFormatContext->streams 中
    // AVFormatContext->streams 是一个数组，数组大小是 AVFormatContext->nb_streams
    if (avformat_find_stream_info(pFmtCtx, NULL) < 0) {
        return -1;
    }

    for (int i = 0; i < pFmtCtx->nb_streams; ++i) {
        if (pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            video_time_base = av_q2d(pFmtCtx->streams[i]->time_base);
            AVCodecParameters *codec_par = pFmtCtx->streams[i]->codecpar;
            width = codec_par->width;
            height = codec_par->height;
        } else if (pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = i;
            audio_time_base = av_q2d(pFmtCtx->streams[i]->time_base);
            AVCodecParameters *codec_par = pFmtCtx->streams[i]->codecpar;

            channels = codec_par->channels;
            sample_rate = codec_par->sample_rate;
            nb_samples = codec_par->frame_size;

        }
    }
    if (videoStream == -1 || audioStream == -1) {
        return -1;
    }

    // 获取解码器上下文
    AVCodecParameters *videoCodecPar = pFmtCtx->streams[videoStream]->codecpar;

    // 获取解码器
    AVCodec *videoCodec = avcodec_find_decoder(videoCodecPar->codec_id);

    if (videoCodec == NULL) {
        std::cerr << "Unsupported codec!" << std::endl;
        return -1;
    }

    // 解码器上下文
    videoCodecContext = avcodec_alloc_context3(NULL);  // allocate
    if (avcodec_parameters_to_context(videoCodecContext, videoCodecPar) < 0) {// initialize
        return -1;
    }
    // 打开解码器
    if (avcodec_open2(videoCodecContext, videoCodec, NULL) < 0) {
        return -1;
    }

    // 获取解码器上下文
    AVCodecParameters *audioCodecPar = pFmtCtx->streams[audioStream]->codecpar;

    // 获取解码器
    AVCodec *audioCodec = avcodec_find_decoder(audioCodecPar->codec_id);
    if (audioCodec == NULL) {
        std::cerr << "Unsupported codec!" << std::endl;
        return -1;
    }

    // 解码器上下文
    audioCodecContext = avcodec_alloc_context3(NULL);  // allocate
    if (avcodec_parameters_to_context(audioCodecContext, audioCodecPar) < 0) {// initialize
        return -1;
    }
    // 打开解码器
    if (avcodec_open2(audioCodecContext, audioCodec, NULL) < 0) {
        return -1;
    }




    // 创建转换后的帧
    frame = av_frame_alloc();




    // 创建 SDL screen
    SDL_Window *screen = SDL_CreateWindow("Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          videoCodecContext->width, videoCodecContext->height,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (screen == NULL) {
        std::cerr << "Could not create window - " << SDL_GetError() << std::endl;
        return -1;
    }
    // 创建 Renderer
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_TARGETTEXTURE);



    // 创建 texture
//    Uint32 sdlPixFormat = SDL_PIXELFORMAT_IYUV;
    Uint32 sdlPixFormat = SDL_PIXELFORMAT_BGR24;
    texture = SDL_CreateTexture(renderer, sdlPixFormat,
                                SDL_TEXTUREACCESS_STREAMING, videoCodecContext->width,
                                videoCodecContext->height);

    videoScalar = new VideoScalar(videoCodecContext->width, videoCodecContext->height,
                                  videoCodecContext->pix_fmt,
                                  videoCodecContext->width, videoCodecContext->height,
                                  AV_PIX_FMT_BGR24
    );

    //初始化音频播放器
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = audioCodecContext->sample_rate;
//    wanted_spec.format = AUDIO_F32;
    wanted_spec.format = AUDIO_S16;
    wanted_spec.channels = audioCodecContext->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = audioCodecContext->frame_size;
    wanted_spec.callback = fill_audio;
    resampler = new AudioResampler(audioCodecContext->sample_fmt, audioCodecContext->channels,
                                   audioCodecContext->sample_rate, AV_SAMPLE_FMT_S16, audioCodecContext->channels,
                                   audioCodecContext->sample_rate);
    if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
        printf("can't open audio.\n");
        return -1;
    }
    //Play
    SDL_PauseAudio(0);


    SDL_Event event;

    AVPacket *packet = av_packet_alloc();

    std::thread t([&]() {
        while (av_read_frame(pFmtCtx, packet) >= 0) {// 将 frame 读取到 packet
            // 迭代结束后释放 av_read_frame 分配的 packet 内存
            if (packet->stream_index == videoStream) { // 如果读到的是视频流
                // 使用解码器 pCodecCtx 将 packet 解码
                avcodec_send_packet(videoCodecContext, packet);
                // 返回 pCodecCtx 解码后的数据，注意只有在解码完整个 frame 时该函数才返回 0
                while (avcodec_receive_frame(videoCodecContext, frame) == 0) {
                    // 渲染图片
                    std::unique_lock<std::mutex> lock(mv);
//                    AVFrame *scale_frame = nullptr;
//                    int ret = videoScalar->doScale(frame, &scale_frame);
//                    if (ret > 0) {
//                        video_frames.push_back(scale_frame);
//                    }

                    AVFrame *scale_frame = av_frame_alloc();
                    av_image_alloc(scale_frame->data, scale_frame->linesize, frame->width, frame->height,
                                   AV_PIX_FMT_RGB24, 1);
                    //bgr    rgb???
                    libyuv::I420ToRGB24(frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1],
                                        frame->data[2], frame->linesize[2], scale_frame->data[0],
                                        scale_frame->linesize[0],
                                        frame->width, frame->height);
                    scale_frame->width = frame->width;
                    scale_frame->height = frame->height;
                    scale_frame->format = AV_PIX_FMT_RGB24;
                    scale_frame->pts = frame->pts;
                    video_frames.push_back(scale_frame);

                }
            } else {
                // 使用解码器 pCodecCtx 将 packet 解码
                avcodec_send_packet(audioCodecContext, packet);
                // 返回 pCodecCtx 解码后的数据，注意只有在解码完整个 frame 时该函数才返回 0
                while (avcodec_receive_frame(audioCodecContext, frame) == 0) {
                    std::unique_lock<std::mutex> lock(ma);
                    AVFrame *resample_frame = nullptr;
                    int ret = resampler->doResample(frame, &resample_frame);
                    if (ret > 0) {
                        audio_frames.push_back(resample_frame);
                    }
                }
            }
            av_packet_unref(packet);
            // 处理消息
            SDL_PollEvent(&event);
            switch (event.type) {
                case SDL_QUIT:
                    break;
            }
        }
    });
    t.join();
    avformat_close_input(&pFmtCtx);
    avcodec_free_context(&videoCodecContext);
    avcodec_free_context(&audioCodecContext);
    av_frame_free(&frame);
    av_packet_free(&packet);
    while (true) {//等待播放完毕
        {
            std::unique_lock<std::mutex> lock_v(mv);
            std::unique_lock<std::mutex> lock_a(ma);
            if (video_frames.empty() && audio_frames.empty()) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    SDL_DestroyWindow(screen);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(texture);
    SDL_CloseAudio();
    SDL_Quit();

    return 0;
}

