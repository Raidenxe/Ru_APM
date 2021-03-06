#include "Drive_V4L2Reader.hpp"

V4L2Tools::V4L2Drive::V4L2Drive(std::string Device, V4l2Info Info)
{
    v4l2d = Info;
    _flag_TargetDevice = Device;
    _flag_CameraFD = open(_flag_TargetDevice.c_str(), O_RDWR | O_NONBLOCK, 0);
    V4L2Log(ioctl(_flag_CameraFD, VIDIOC_QUERYCAP, &v4l2.CameraInfo), _v4l2_query_error);
#ifdef DEBUG
    std::cout << "DriveInfo: \n"
              << "  Drive      :" << v4l2.CameraInfo.driver << "\n"
              << "  Card       :" << v4l2.CameraInfo.card << "\n"
              << "  Bus        :" << v4l2.CameraInfo.bus_info << "\n"
              << "  Version    :" << v4l2.CameraInfo.version << "\n"
              << "  Capbilities:" << v4l2.CameraInfo.capabilities << "\n";
#endif
    v4l2.CameraFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2.CameraFormat.fmt.pix.width = v4l2d.ImgWidth;
    v4l2.CameraFormat.fmt.pix.height = v4l2d.ImgHeight;
    v4l2.CameraFormat.fmt.pix.pixelformat = v4l2d.PixFormat;
    v4l2.CameraFormat.fmt.pix.field = V4L2_FIELD_INTERLACED;
    V4L2Log(ioctl(_flag_CameraFD, VIDIOC_S_FMT, &v4l2.CameraFormat), _v4l2_vidioc_s_error);
    V4L2Log(ioctl(_flag_CameraFD, VIDIOC_G_FMT, &v4l2.CameraFormat), _v4l2_vidioc_g_error);
#ifdef DEBUG
    std::cout << "DeviceStatus: \n"
              << "  Width:  " << v4l2.CameraFormat.fmt.pix.width << "\n"
              << "  Height: " << v4l2.CameraFormat.fmt.pix.height << "\n"
              << "  PixFmt: " << v4l2.CameraFormat.fmt.pix.pixelformat << "\n";
#endif
    //
    v4l2.CameraReqBuffer.count = v4l2d.FrameBuffer;
    v4l2.CameraReqBuffer.memory = V4L2_MEMORY_MMAP;
    v4l2.CameraReqBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    V4L2Log(ioctl(_flag_CameraFD, VIDIOC_REQBUFS, &v4l2.CameraReqBuffer), _v4l2_reqbuff_error);
    v4l2Buffers = (void **)calloc(v4l2.CameraReqBuffer.count, sizeof(*v4l2Buffers));
    for (int Index = 0; Index < v4l2.CameraReqBuffer.count; ++Index)
    {
        memset(&v4l2.CameraQBuffer, 0, sizeof(v4l2.CameraQBuffer));
        v4l2.CameraQBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2.CameraQBuffer.memory = V4L2_MEMORY_MMAP;
        v4l2.CameraQBuffer.index = Index;
        V4L2Log(ioctl(_flag_CameraFD, VIDIOC_QUERYBUF, &v4l2.CameraQBuffer), _v4l2_querybuff_error);
        v4l2Buffers[Index] = mmap(NULL, v4l2.CameraQBuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, _flag_CameraFD, v4l2.CameraQBuffer.m.offset);
        if (MAP_FAILED == v4l2Buffers[Index])
        {
#ifdef DEBUG
            std::cout << "  Mapping ERROR"
                      << "\n";
#endif
        }
    }
    //
    for (int i = 0; i < v4l2.CameraReqBuffer.count; ++i)
    {
        memset(&v4l2.CameraBuffer, 0, sizeof(v4l2.CameraBuffer));
        v4l2.CameraBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2.CameraBuffer.memory = V4L2_MEMORY_MMAP;
        v4l2.CameraBuffer.index = i;
        V4L2Log(ioctl(_flag_CameraFD, VIDIOC_QBUF, &v4l2.CameraBuffer), _v4l2_qbuf_error);
    }
    memset(&v4l2.CameraBuffer, 0, sizeof(v4l2.CameraBuffer));
    v4l2.CameraBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2.CameraBuffer.memory = V4L2_MEMORY_MMAP;
    v4l2.CameraBuffer.index = 0;
    V4L2Log(ioctl(_flag_CameraFD, VIDIOC_STREAMON, &v4l2.CameraBuffer.type), _v4l2_vidioc_streamon_error);
}

bool V4L2Tools::V4L2Drive::V4L2Control(unsigned int id, int value)
{
    v4l2.CameraContorl.id = id;
    v4l2.CameraContorl.value = value;
    if (ioctl(_flag_CameraFD, VIDIOC_S_CTRL, &v4l2.CameraContorl) == -1)
    {
        return false;
    }
    return true;
}

unsigned char *V4L2Tools::V4L2Drive::V4L2Read()
{
    if (v4l2d.Is_fastMode & v4l2d.FrameBuffer == 1)
    {
        if (ioctl(_flag_CameraFD, VIDIOC_DQBUF, &v4l2.CameraBuffer) != -1)
        {
            ioctl(_flag_CameraFD, VIDIOC_QBUF, &v4l2.CameraBuffer);
        }
    }
    else
    {
        for (;;)
        {
            fd_set fds;
            struct timeval tv;
            int r;
            FD_ZERO(&fds);
            FD_SET(_flag_CameraFD, &fds);
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            r = select(_flag_CameraFD + 1, &fds, NULL, NULL, &tv);
            if (ioctl(_flag_CameraFD, VIDIOC_DQBUF, &v4l2.CameraBuffer) != -1)
            {
                V4L2Log(ioctl(_flag_CameraFD, VIDIOC_QBUF, &v4l2.CameraBuffer), _v4l2_camread_error);
                break;
            }
        }
    }
    return (unsigned char *)v4l2Buffers[v4l2.CameraBuffer.index];
}

V4L2Tools::V4L2Drive::~V4L2Drive()
{
    memset(&v4l2.CameraBuffer, 0, sizeof(v4l2.CameraBuffer));
    v4l2.CameraBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(_flag_CameraFD, VIDIOC_STREAMOFF, &v4l2.CameraBuffer.type);
    for (int i = 0; i < v4l2d.FrameBuffer; i++)
    {
        munmap(v4l2Buffers[i], v4l2.CameraQBuffer.length);
    }
    free(v4l2Buffers);
}

void V4L2Tools::V4L2Drive::yuyv2rgb24(const unsigned char *src, unsigned char *dest, int width, int height)
{
    int j;

    while (--height >= 0)
    {
        for (j = 0; j + 1 < width; j += 2)
        {
            int u = src[1];
            int v = src[3];
            int u1 = (((u - 128) << 7) + (u - 128)) >> 6;
            int rg = (((u - 128) << 1) + (u - 128) +
                      ((v - 128) << 2) + ((v - 128) << 1)) >>
                     3;
            int v1 = (((v - 128) << 1) + (v - 128)) >> 1;

            *dest++ = CLIP(src[0] + v1);
            *dest++ = CLIP(src[0] - rg);
            *dest++ = CLIP(src[0] + u1);

            *dest++ = CLIP(src[2] + v1);
            *dest++ = CLIP(src[2] - rg);
            *dest++ = CLIP(src[2] + u1);
            src += 4;
        }
        src += v4l2.CameraFormat.fmt.pix.bytesperline - (width * 2);
    }
}

void V4L2Tools::V4L2Drive::V4L2Log(int signal, int error)
{
    if (signal == -1)
    {
        throw error;
    }
}