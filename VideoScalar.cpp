//
// Created by ZMY on 2019/3/3.
//

#include "VideoScalar.h"


VideoScalar::VideoScalar(int srcWidth, int srcHeight, AVPixelFormat srcFmt, int dstWidth,
                         int dstHeight,
                         AVPixelFormat dstFormat) {
    this->dstWidth = dstWidth;
    this->dstHeight = dstHeight;
    this->dstFormat = dstFormat;
    this->srcWidth = srcWidth;
    this->srcHeight = srcHeight;
    this->srcFmt = srcFmt;
    sws_ctx = sws_getContext(this->srcWidth,
                             this->srcHeight,
                             this->srcFmt,
                             dstWidth, dstHeight, dstFormat,
                             SWS_BICUBIC, NULL, NULL, NULL);
}


int VideoScalar::doScale(AVFrame *input, AVFrame **output) {
    AVFrame *target = av_frame_alloc();
    av_image_alloc(target->data, target->linesize, dstWidth, dstHeight, dstFormat, 1);
    int ret = sws_scale(sws_ctx, (uint8_t const *const *) input->data, input->linesize, 0,
                        srcHeight,
                        target->data, target->linesize);
    if (ret < 0) {
        printf("sws_scale fail\n");
        av_free(target->data[0]);
        av_frame_free(&target);
        return ret;
    }
    target->width = dstWidth;
    target->height = dstHeight;
    target->format = dstFormat;
    target->pts = input->pts;
    *output = target;
    return ret;
}


VideoScalar::~VideoScalar() {
    release();
}

void VideoScalar::release() {
    sws_freeContext(sws_ctx);
}
