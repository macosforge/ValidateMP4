#
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
#

VPATH = . ../src

CFLAGS = -g -DLITTLEENDIAN -Wno-multichar

CC = gcc

HEADERS = \
ValidateMP4.h \
EndianMP4.h

SOURCES = \
ValidateAtomList.c \
ValidateAtoms.c \
ValidateBitStreams.c \
ValidateBits.c \
ValidateFileIO.c \
ValidateHints.c \
ValidateMP4.c

OBJECTS := $(patsubst %.c,%.o,$(SOURCES))

ValidateMP4:	$(OBJECTS) $(HEADERS)
	$(CC) -g -o $@ $(CFLAGS) $(OBJECTS)
	
clean:
	-rm $(OBJECTS) $(SOURCES:.c=.d) ValidateMP4


%.d: %.c
	$(SHELL) -ec '$(CC) -M $(CFLAGS) $< | sed '\''s/$*.o/& $@/g'\'' > $@'

TAGS: $(SOURCES)
	etags $^

#
# Include all dependency files
#
-include $(SOURCES:.c=.d)
