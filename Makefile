steamcontrollersinger : main.cpp midifile/midifile.c
	g++ -o steamcontrollersinger main.cpp midifile/midifile.c -lusb-1.0 -fpermissive -std=c++11
