# Make file to  for  encoder library for Linux platform
#
#
#--------------------------------------
.PHONY: CREATE_DIR clean all


ifeq ($(ARM), 0)
CFLAGS += -O2 -std=c99
else
CFLAGS += -DARM
endif

CFLAGS += -fPIC -g

LDFLAGS =

#CFLAGS += -static
#CLFAGS += -Wno-implicit-function-declaration -Wno-format -Wl,--fatal-warning
#CLFAGS += -Wall -Werror

INC = -I./include -I. -I../vpuapi/include -I../vpuapi


CFLAGS  +=  $(INC)
ARFLAGS += rcs
TARGET = aml_enc_test

LDLIBS += -lm -lrt -lvpcodec -lamvenc_api
# pthread
LDLIBS += -lpthread -lion


#LDFLAGS += -shared

OBJDIR=obj
ALLOBJS=*.o
ALLDEPS=*.dep
ALLLIBS=*.a
RM=rm -f
MKDIR=mkdir -p

SOURCES = test.c test_dma.c
OBJECNAMES = test.o test_dma.o

OBJECTNAMES=$(patsubst %.c,%.o, $(SOURCES))
OBJECTPATHS=$(addprefix $(OBJDIR)/,$(notdir $(OBJECTNAMES)))


$(TARGET): CREATE_DIR $(OBJECTPATHS)
	$(CC) -o $(TARGET) $(OBJECTPATHS) $(LDFLAGS) $(LDLIBS)

CREATE_DIR:
	$(MKDIR) $(OBJDIR)

$(OBJDIR)/%.o:%.c CREATE_DIR
	$(CC) $(LDFLAGS) -c $< -o $@

-include $(OBJECTPATHS:.o=.dep)

clean:
	$(RM) $(TARGET)
	$(RM) $(OBJDIR)/$(ALLOBJS)
	$(RM) $(OBJDIR)/$(ALLDEPS)


