# ==== Build Config  ========================================================

# Output executable name
EXEC ?= alchitry-loader

# When building a package or installing otherwise in the system, make
# sure that the variable PREFIX is defined, e.g. make PREFIX=/usr/local
PREFIX = /usr/local

# Build a version string
GIT_VERSION := "$(shell git describe --abbrev=4 --dirty --always --tags)"

LFLAGS = -lpthread
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
	LFLAGS += -lobjc -framework IOKit -framework CoreFoundation
	PLATFORM_ARCH = osx
else ifeq ($(UNAME), Linux)
	PLATFORM_ARCH = linux
else
	# TODO : Windows build flags
	LFLAGS += 
	PLATFORM_ARCH = windows
endif

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src ./lib/$(PLATFORM_ARCH)
DRIVER_DIR ?= ./driver

SRCS := $(shell find $(SRC_DIRS) -name *.cpp)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
S_LIBS := $(shell find $(SRC_DIRS) -name *.a)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CXX = g++
CXXFLAGS += $(INC_FLAGS) -DVERSION=\"$(GIT_VERSION)\" -MMD -MP -std=c++11
LFLAGS += $(S_LIBS)

# ==== Build Types ============================================================

.PHONY: all clean install uninstall

all: release

release: CXXFLAGS += -O2
release: $(BUILD_DIR)/$(EXEC)

debug: CXXFLAGS += -Og -g -DDEBUG
debug: $(BUILD_DIR)/$(EXEC)


# ==== Build Chain ============================================================

$(BUILD_DIR)/$(EXEC): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(CXXFLAGS) $(LFLAGS)

$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

# ==== Install and Uninstall tools ============================================

define install_tool
install $(1) $(PREFIX)/bin/ ;
endef

install: $(BUILD_DIR)/$(EXEC)
ifndef PREFIX
	$(error PREFIX is not set)
endif
	@mkdir -p $(PREFIX)/bin
	$(call install_tool,$^)
ifeq ($(UNAME), Linux)
	@mkdir -p /etc/udev/rules.d/
	cp $(DRIVER_DIR)/*.rules /etc/udev/rules.d/
endif

define uninstall_tool
rm -Rf $(PREFIX)/bin/$(1) ;
endef

uninstall:
ifndef PREFIX
	$(error PREFIX is not set)
endif
	$(call uninstall_tool,$(EXEC))
ifeq ($(UNAME), Linux)
	rm -Rf etc/udev/rules.d/99-alchitry.rules
endif

# ==== Cleanup ================================================================

.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)
