# C compiler options
CC	= gcc
#CFLAGS	= -g -O2             
TARGET	= libEasyLibusb.so
LIBSPATH = ../lib
LIBS	= -lusb-1.0
INC	= /usr/local/include/libusb-1.0
 
#多字节字符 指定汉字编码方式GB18030
EXCHAR	= -fexec-charset=GB18030    
 
# Source files
SRCS = main.c
 
# Object files
OBJS	= $(SRCS:.c=.o)
 
# Make everything
all:	$(TARGET) 
 
# Make the application
#-lstdc++:编译c++的时候用到
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(EXCHAR) -o $(TARGET) $(OBJS)  $(LIBS)  -shared -fPIC 
 
# Dependencies
#$@:表示目标文件，即：demo.o
#$<：表示依赖文件，即：demo.c
#-I$(INC) :指向包含了的.h文件的路径 即wbe,h的路径
$(OBJS): %.o: %.c 
	$(CC) -c $(CFLAGS) $(EXCHAR) -o $@ $< -I$(INC)  
#
# Clean all object files...
#
 
clean:
	$(RM) $(OBJS) $(TARGET) 

