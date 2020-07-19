//
// Created by ZMY on 2019/3/3.
//

#include "AudioResampler.h"


AudioResampler::AudioResampler(AVSampleFormat in_sample_fmt,
                               int in_channels,
                               int in_sample_rate, AVSampleFormat out_sample_fmt,
                               int out_channels,
                               int out_sample_rate) : out_sample_fmt(out_sample_fmt),
                                                      out_channels(out_channels),
                                                      out_sample_rate(out_sample_rate) {
    int ret;
    swr_ctx = swr_alloc_set_opts(nullptr,
                                 av_get_default_channel_layout(out_channels), out_sample_fmt,
                                 out_sample_rate,
                                 av_get_default_channel_layout(in_channels), in_sample_fmt,
                                 in_sample_rate,
                                 0, nullptr);
    ret = swr_init(swr_ctx);
    if (ret < 0) {
        printf("swr_init fail\n");
    }

}


int AudioResampler::doResample(AVFrame *input, AVFrame **output) {
    AVFrame *target = av_frame_alloc();

    //计算出输出音频帧的采样数，直接引用源采样数会导致每一帧的播放时间不对，且有杂音
    int out_nb_samples = (input->nb_samples / (double) input->sample_rate) * out_sample_rate;

    av_samples_alloc(target->data, target->linesize, out_channels, out_nb_samples, out_sample_fmt,
                     1);
    int ret = swr_convert(swr_ctx, target->data, target->linesize[0],
                          (const uint8_t **) (input->data),
                          input->nb_samples);
    if (ret < 0) {
        av_free(target->data[0]);
        av_frame_free(&target);
        return ret;
    }
    target->pts = input->pts;
    target->nb_samples = out_nb_samples;
    target->sample_rate = out_sample_rate;
    target->format = out_sample_fmt;
    *output = target;
    return ret;
}

AudioResampler::~AudioResampler() {
    release();
}

void AudioResampler::release() {
    swr_free(&swr_ctx);
}







