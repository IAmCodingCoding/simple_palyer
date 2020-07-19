//
// Created by ZMY on 2019/3/3.
//

#ifndef VIDEOTRANSFORMER_AUDIORESAMPLER_H
#define VIDEOTRANSFORMER_AUDIORESAMPLER_H
extern "C" {
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
};

class AudioResampler {
private:
    AVSampleFormat out_sample_fmt;
    int out_channels;
    int out_sample_rate;

    SwrContext *swr_ctx = NULL;
    uint64_t src_nb_channels;


public:
    AudioResampler(AVSampleFormat in_sample_fmt,
                   int in_channels,
                   int in_sample_rate, AVSampleFormat out_sample_fmt,
                   int out_channels,
                   int out_sample_rate);

    int doResample(AVFrame *input, AVFrame **output);

    void release();

    virtual ~AudioResampler();
};


#endif //VIDEOTRANSFORMER_AUDIORESAMPLER_H
