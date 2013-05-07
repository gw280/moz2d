# Simple checks
HAS_CAIRO=$(shell pkg-config cairo && echo YES)
ifneq ($(HAS_CAIRO),YES)
$(error Cairo package not found)
endif

CXXFLAGS += `pkg-config --cflags cairo`
LIBS += `pkg-config --libs cairo`
DEFINES  += USE_CAIRO

MOZ2D_CPPSRCS += \
  DrawTargetCairo.cpp \
  PathCairo.cpp \
  SourceSurfaceCairo.cpp \
  $(NULL)

TESTING_CPPSRCS += \
  perftest/TestDrawTargetCairoImage.cpp \
  $(NULL)

