# 03/05/2018

# Optional definitions:
# -DWRITE_IN_SENDER
# -DSEND_COUNT

CFLAGS = -Wall
LDLIBS = -lpthread

all: mirror measurer

mirror: mirror.c

measurer: msgctx.o result_buffer.o writer.o receiver.o storer.o sender.o \
          thread_context.o single_thread.o multi_thread.o measurer.o

measurer.o: writer.h receiver.h storer.h sender.h \
            measurer_elements.h thread_context.h single_thread.h \
            multi_thread.h send_history.h result_buffer.h measurer.c

result_buffer.o: result_buffer.h result_buffer.c

single_thread.o: receiver.h storer.h sender.h \
                 measurer_elements.h single_thread.h single_thread.c

multi_thread.o: receiver.h storer.h sender.h \
                measurer_elements.h thread_context.h multi_thread.c

thread_context.o: thread_context.h thread_context.c

msgctx.o: msgctx.h msgctx.c

writer.o: writer.h writer.c
receiver.o: send_history.h result_buffer.h msgctx.h \
            time_common.h receiver.h receiver.c
storer.o: send_history.h msgctx.h time_common.h \
          storer.h storer.c
# -DWRITE_IN_SENDER implies result_buffer.h time_common.h
sender.o: send_history.h result_buffer.h time_common.h \
          sender.h sender.c
