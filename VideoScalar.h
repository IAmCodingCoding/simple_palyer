//
// Created by ZMY on 2019/3/3.
//

#ifndef VIDEOTRANSFORMER_VIDEOSCALER_H
#define VIDEOTRANSFORMER_VIDEOSCALER_H


extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
};


class VideoScalar {
private:


    SwsContext *sws_ctx = nullptr;
    int dstWidth;
    int dstHeight;
    AVPixelFormat dstFormat;
    int srcWidth;
    int srcHeight;
    AVPixelFormat srcFmt;

public:

    VideoScalar(int srcWidth, int srcHeight, AVPixelFormat srcFmt, int dstWidth, int dstHeight,
                AVPixelFormat dstFormat);

    int doScale(AVFrame *input, AVFrame **output);

    void release();

    virtual ~VideoScalar();


};


#endif //VIDEOTRANSFORMER_VIDEOSCALER_H
