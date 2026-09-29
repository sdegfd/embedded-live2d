# ESP32-S3 port

There is no S3-only driver in this directory. The S3 example links the same `live2d_engine` component as P4 and the software RGB565 helper in `include/l2d/l2d_image.h`.

The example is `examples/esp32s3`. It does not register PPA, DSI, or a panel. A successful `idf.py build` is build verification only.
