# project
TARGET   = sentry

# directories
BINDIR   = bin
SRCDIR   = src
LIBDIR   = lib
OBJDIR   = $(BINDIR)
UTDIR    = ut

# compiler and linker
CC       = g++
LIBS     = -lm -lpthread -lssl -lcrypto -lopencv_core -lopencv_highgui -lraspicam -lraspicam_cv -lwiiusecpp
UTLIBS   = -lm -lpthread -lssl -lcrypto -lopencv_core -lopencv_highgui
INCLUDES = -I$(SRCDIR)

# check target
ifeq ($(lastword $(MAKECMDGOALS)), release)
FLAGS    = -Wall -Werror -std=c++11 -O2
LDFLAGS  = -L$(LIBDIR)
else ifeq ($(lastword $(MAKECMDGOALS)), profile)
FLAGS    = -Wall -Werror -std=c++11 -O0 -g -ggdb -rdynamic -DPROFILE -fprofile-arcs -ftest-coverage -fno-omit-frame-pointer
LDFLAGS  = -L$(LIBDIR) -fprofile-arcs
LIBS    := $(LIBS) -lgcov
else
FLAGS    = -Wall -Werror -std=c++11 -O0 -g -ggdb -rdynamic -DDEBUG
LDFLAGS  = -L$(LIBDIR)
endif

# main entry point
all release profile: $(BINDIR)/$(TARGET) $(BINDIR)/netcom-client

# build the application
$(BINDIR)/$(TARGET): $(OBJDIR)/sentry.o $(OBJDIR)/framework.o $(OBJDIR)/message.o \
                     $(OBJDIR)/message_queue.o $(OBJDIR)/worker.o $(OBJDIR)/camera.o \
                     $(OBJDIR)/rcmgr.o $(OBJDIR)/chmgr.o $(OBJDIR)/netcom.o $(OBJDIR)/engine.o
	$(CC) -o $@ $^ $(LDFLAGS) $(INCLUDES) $(LIBS)
$(OBJDIR)/sentry.o: $(SRCDIR)/sentry.cc $(SRCDIR)/engine.h $(SRCDIR)/message.h $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)
$(OBJDIR)/framework.o: $(SRCDIR)/framework.cc $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)
$(OBJDIR)/message.o: $(SRCDIR)/message.cc $(SRCDIR)/message.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)
$(OBJDIR)/message_queue.o: $(SRCDIR)/message_queue.cc $(SRCDIR)/message_queue.h \
                           $(SRCDIR)/message.h $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)
$(OBJDIR)/worker.o: $(SRCDIR)/worker.cc $(SRCDIR)/worker.h $(SRCDIR)/message_queue.h \
                    $(SRCDIR)/message.h $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)
$(OBJDIR)/camera.o: $(SRCDIR)/camera.cc $(SRCDIR)/camera.h \
                    $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)
$(OBJDIR)/rcmgr.o: $(SRCDIR)/rcmgr.cc $(SRCDIR)/rcmgr.h $(SRCDIR)/message_queue.h \
	               $(SRCDIR)/message.h $(SRCDIR)/worker.h $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -fPIC -funroll-loops -fpermissive -o $@ -c $< $(INCLUDES)
$(OBJDIR)/chmgr.o: $(SRCDIR)/chmgr.cc $(SRCDIR)/chmgr.h $(SRCDIR)/message_queue.h \
                   $(SRCDIR)/message.h $(SRCDIR)/worker.h $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)
$(OBJDIR)/netcom.o: $(SRCDIR)/netcom.cc $(SRCDIR)/netcom.h $(SRCDIR)/camera.h \
                    $(SRCDIR)/message_queue.h $(SRCDIR)/message.h \
                    $(SRCDIR)/worker.h $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)
$(OBJDIR)/engine.o: $(SRCDIR)/engine.cc $(SRCDIR)/engine.h $(SRCDIR)/camera.h \
                    $(SRCDIR)/rcmgr.h $(SRCDIR)/chmgr.h $(SRCDIR)/netcom.h \
                    $(SRCDIR)/message_queue.h $(SRCDIR)/message.h $(SRCDIR)/worker.h \
                    $(SRCDIR)/framework.h
	$(CC) $(FLAGS) -fpermissive -o $@ -c $< $(INCLUDES)

# netcom client for unit testing
$(BINDIR)/netcom-client: $(OBJDIR)/netcom-client.o $(OBJDIR)/message.o
	$(CC) -o $@ $^ $(FLAGS) $(INCLUDES) $(UTLIBS)
$(OBJDIR)/netcom-client.o: $(UTDIR)/netcom-client.cc $(SRCDIR)/message.h
	$(CC) $(FLAGS) -o $@ -c $< $(INCLUDES)

# clean up object files
.PHONEY: clean
clean:
	rm -f $(OBJDIR)/*.o

# generate doxygen
.PHONEY: doc
doc: $(SRCDIR) Doxyfile
	doxygen Doxyfile
