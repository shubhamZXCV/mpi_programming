CXX        = g++
MPICXX     = mpicxx
CXXFLAGS   = -O2 -Wall

all: main_seq main_distri

main_seq: main_seq.cpp
	$(CXX) $(CXXFLAGS) -o main_seq main_seq.cpp

main_distri: main_distri.cpp
	$(MPICXX) $(CXXFLAGS) -o main_distri main_distri.cpp

clean:
	rm -f main_seq main_distri *.o

.PHONY: all clean
