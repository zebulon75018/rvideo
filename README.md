# rvideo
A qt application to compare and annotate video...

#dependency 
LIBS+= -lopencv_core -lopencv_imgproc  -lopencv_videoio
LIBS+= -lavcodec -lavformat -lavdevice -lavutil -lm

and webengine.

# compil

qmake  rvideo.pro

dependency : libqt5webkit5-dev
libopencv-core4.5d 
![Image](rvideo.png)
