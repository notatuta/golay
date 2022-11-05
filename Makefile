all: test

golay.o: golay.cpp golay.h
	g++ -O3 -Wall -Wextra -std=c++17 -c -g golay.cpp

golay: golay.o
	g++ -static -g -o golay golay.o

golay.exe: golay.cpp golay.h
	/usr/bin/x86_64-w64-mingw32-g++-win32 -O3 -Wall -Wextra -std=c++17 -static -g -o golay.exe golay.cpp

test: golay
	cp golay.cpp test.file
	./golay test.file
	mv test.file reference.file
	rm test.file.Golay_F # intentionally drop one slice
	./golay test.file.Golay_A
	@echo input.file and reference.file should match
	diff -s test.file reference.file

clean:
	-rm golay.o test.file test.file.Golay_A test.file.Golay_B test.file.Golay_C \
	test.file.Golay_D test.file.Golay_E test.file.Golay_G test.file.Golay_H \
	reference.file

